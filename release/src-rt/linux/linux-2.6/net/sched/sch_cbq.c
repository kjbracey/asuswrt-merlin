/*
 * net/sched/sch_cbq.c	Class-Based Queueing discipline.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 */

#include <linux/module.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/notifier.h>
#include <net/ip.h>
#include <net/netlink.h>
#include <net/route.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/pkt_sched.h>


/*	Class-Based Queueing (CBQ) algorithm.
	=======================================

	Sources: [1] Sally Floyd and Van Jacobson, "Link-sharing and Resource
		 Management Models for Packet Networks",
		 IEEE/ACM Transactions on Networking, Vol.3, No.4, 1995

		 [2] Sally Floyd, "Notes on CBQ and Guaranteed Service", 1995

		 [3] Sally Floyd, "Notes on Class-Based Queueing: Setting
		 Parameters", 1996

		 [4] Sally Floyd and Michael Speer, "Experimental Results
		 for Class-Based Queueing", 1998, not published.

	-----------------------------------------------------------------------

	Algorithm skeleton was taken from NS simulator cbq.cc.
	If someone wants to check this code against the LBL version,
	he should take into account that ONLY the skeleton was borrowed,
	the implementation is different. Particularly:

	--- The WRR algorithm is different. Our version looks more
	reasonable (I hope) and works when quanta are allowed to be
	less than MTU, which is always the case when real time classes
	have small rates. Note, that the statement of [3] is
	incomplete, delay may actually be estimated even if class
	per-round allotment is less than MTU. Namely, if per-round
	allotment is W*r_i, and r_1+...+r_k = r < 1

	delay_i <= ([MTU/(W*r_i)]*W*r + W*r + k*MTU)/B

	In the worst case we have IntServ estimate with D = W*r+k*MTU
	and C = MTU*r. The proof (if correct at all) is trivial.


	--- It seems that cbq-2.0 is not very accurate. At least, I cannot
	interpret some places, which look like wrong translations
	from NS. Anyone is advised to find these differences
	and explain to me, why I am wrong 8).

	--- Linux has no EOI event, so that we cannot estimate true class
	idle time. Workaround is to consider the next dequeue event
	as sign that previous packet is finished. This is wrong because of
	internal device queueing, but on a permanently loaded link it is true.
	Moreover, combined with clock integrator, this scheme looks
	very close to an ideal solution.  */

struct cbq_sched_data;


struct cbq_class
{
	struct cbq_class	*next;		/* hash table link */
	struct cbq_class	*next_alive;	/* next class with backlog in this priority band */

/* Parameters */
	u32			classid;
	unsigned char		priority;	/* class priority */
	unsigned char		priority2;	/* priority to be used after overlimit */
	unsigned char		ewma_log;	/* time constant for idle time calculation */
	unsigned char		ovl_strategy;
#ifdef CONFIG_NET_CLS_POLICE
	unsigned char		police;
#endif

	u32			defmap;

	/* Link-sharing scheduler parameters */
	long			maxidle;	/* Class parameters: see below. */
	long			offtime;
	long			minidle;
	u32			avpkt;
	struct qdisc_rate_table	*R_tab;

	/* Overlimit strategy parameters */
	void			(*overlimit)(struct cbq_class *cl);
	psched_tdiff_t		penalty;

	/* General scheduler (WRR) parameters */
	long			allot;
	long			quantum;	/* Allotment per WRR round */
	long			weight;		/* Relative allotment: see below */

	struct Qdisc		*qdisc;		/* Ptr to CBQ discipline */
	struct cbq_class	*split;		/* Ptr to split node */
	struct cbq_class	*share;		/* Ptr to LS parent in the class tree */
	struct cbq_class	*tparent;	/* Ptr to tree parent in the class tree */
	struct cbq_class	*borrow;	/* NULL if class is bandwidth limited;
						   parent otherwise */
	struct cbq_class	*sibling;	/* Sibling chain */
	struct cbq_class	*children;	/* Pointer to children chain */

	struct Qdisc		*q;		/* Elementary queueing discipline */


/* Variables */
	unsigned char		cpriority;	/* Effective priority */
	unsigned char		delayed;
	unsigned char		level;		/* level of the class in hierarchy:
						   0 for leaf classes, and maximal
						   level of children + 1 for nodes.
						 */

	psched_time_t		last;		/* Last end of service */
	psched_time_t		undertime;
	long			avgidle;
	long			deficit;	/* Saved deficit for WRR */
	psched_time_t		penalized;
	struct gnet_stats_basic bstats;
	struct gnet_stats_queue qstats;
	struct gnet_stats_rate_est rate_est;
	spinlock_t		*stats_lock;
	struct tc_cbq_xstats	xstats;

	struct tcf_proto	*filter_list;

	int			refcnt;
	int			filters;

	struct cbq_class 	*defaults[TC_PRIO_MAX+1];
};

struct cbq_sched_data
{
	struct cbq_class	*classes[16];		/* Hash table of all classes */
	int			nclasses[TC_CBQ_MAXPRIO+1];
	unsigned		quanta[TC_CBQ_MAXPRIO+1];

	struct cbq_class	link;

	unsigned		activemask;
	struct cbq_class	*active[TC_CBQ_MAXPRIO+1];	/* List of all classes
								   with backlog */

#ifdef CONFIG_NET_CLS_POLICE
	struct cbq_class	*rx_class;
#endif
	struct cbq_class	*tx_class;
	struct cbq_class	*tx_borrowed;
	int			tx_len;
	psched_time_t		now;		/* Cached timestamp */
	psched_time_t		now_rt;		/* Cached real time */
	unsigned		pmask;

	struct hrtimer		delay_timer;
	struct qdisc_watchdog	watchdog;	/* Watchdog timer,
						   started when CBQ has
						   backlog, but cannot
						   transmit just now */
	psched_tdiff_t		wd_expires;
	int			toplevel;
	u32			hgenerator;
};


#define L2T(cl,len)	qdisc_l2t((cl)->R_tab,len)


static __inline__ unsigned cbq_hash(u32 h)
{
	h ^= h>>8;
	h ^= h>>4;
	return h&0xF;
}

static __inline__ struct cbq_class *
cbq_class_lookup(struct cbq_sched_data *q, u32 classid)
{
	struct cbq_class *cl;

	for (cl = q->classes[cbq_hash(classid)]; cl; cl = cl->next)
		if (cl->classid == classid)
			return cl;
	return NULL;
}

#ifdef CONFIG_NET_CLS_POLICE

static struct cbq_class *
cbq_reclassify(struct sk_buff *skb, struct cbq_class *this)
{
	struct cbq_class *cl, *new;

	for (cl = this->tparent; cl; cl = cl->tparent)
		if ((new = cl->defaults[TC_PRIO_BESTEFFORT]) != NULL && new != this)
			return new;

	return NULL;
}

#endif

/* Classify packet. The procedure is pretty complicated, but
   it allows us to combine link sharing and priority scheduling
   transparently.

   Namely, you can put link sharing rules (f.e. route based) at root of CBQ,
   so that it resolves to split nodes. Then packets are classified
   by logical priority, or a more specific classifier may be attached
   to the split node.
 */

static struct cbq_class *
cbq_classify(struct sk_buff *skb, struct Qdisc *sch, int *qerr)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *head = &q->link;
	struct cbq_class **defmap;
	struct cbq_class *cl = NULL;
	u32 prio = skb->priority;
	struct tcf_result res;

	/*
	 *  Step 1. If skb->priority points to one of our classes, use it.
	 */
	if (TC_H_MAJ(prio^sch->handle) == 0 &&
	    (cl = cbq_class_lookup(q, prio)) != NULL)
		return cl;

	*qerr = NET_XMIT_BYPASS;
	for (;;) {
		int result = 0;
		defmap = head->defaults;

		/*
		 * Step 2+n. Apply classifier.
		 */
		if (!head->filter_list || (result = tc_classify(skb, head->filter_list, &res)) < 0)
			goto fallback;

		if ((cl = (void*)res.class) == NULL) {
			if (TC_H_MAJ(res.classid))
				cl = cbq_class_lookup(q, res.classid);
			else if ((cl = defmap[res.classid&TC_PRIO_MAX]) == NULL)
				cl = defmap[TC_PRIO_BESTEFFORT];

			if (cl == NULL || cl->level >= head->level)
				goto fallback;
		}

#ifdef CONFIG_NET_CLS_ACT
		switch (result) {
		case TC_ACT_QUEUED:
		case TC_ACT_STOLEN:
			*qerr = NET_XMIT_SUCCESS;
		case TC_ACT_SHOT:
			return NULL;
		}
#elif defined(CONFIG_NET_CLS_POLICE)
		switch (result) {
		case TC_POLICE_RECLASSIFY:
			return cbq_reclassify(skb, cl);
		case TC_POLICE_SHOT:
			return NULL;
		default:
			break;
		}
#endif
		if (cl->level == 0)
			return cl;

		/*
		 * Step 3+n. If classifier selected a link sharing class,
		 *	   apply agency specific classifier.
		 *	   Repeat this procdure until we hit a leaf node.
		 */
		head = cl;
	}

fallback:
	cl = head;

	/*
	 * Step 4. No success...
	 */
	if (TC_H_MAJ(prio) == 0 &&
	    !(cl = head->defaults[prio&TC_PRIO_MAX]) &&
	    !(cl = head->defaults[TC_PRIO_BESTEFFORT]))
		return head;

	return cl;
}

/*
   A packet has just been enqueued on the empty class.
   cbq_activate_class adds it to the tail of active class list
   of its priority band.
 */

static __inline__ void cbq_activate_class(struct cbq_class *cl)
{
	struct cbq_sched_data *q = qdisc_priv(cl->qdisc);
	int prio = cl->cpriority;
	struct cbq_class *cl_tail;

	cl_tail = q->active[prio];
	q->active[prio] = cl;

	if (cl_tail != NULL) {
		cl->next_alive = cl_tail->next_alive;
		cl_tail->next_alive = cl;
	} else {
		cl->next_alive = cl;
		q->activemask |= (1<<prio);
	}
}

/*
   Unlink class from active chain.
   Note that this same procedure is done directly in cbq_dequeue*
   during round-robin procedure.
 */

static void cbq_deactivate_class(struct cbq_class *this)
{
	struct cbq_sched_data *q = qdisc_priv(this->qdisc);
	int prio = this->cpriority;
	struct cbq_class *cl;
	struct cbq_class *cl_prev = q->active[prio];

	do {
		cl = cl_prev->next_alive;
		if (cl == this) {
			cl_prev->next_alive = cl->next_alive;
			cl->next_alive = NULL;

			if (cl == q->active[prio]) {
				q->active[prio] = cl_prev;
				if (cl == q->active[prio]) {
					q->active[prio] = NULL;
					q->activemask &= ~(1<<prio);
					return;
				}
			}
			return;
		}
	} while ((cl_prev = cl) != q->active[prio]);
}

static void
cbq_mark_toplevel(struct cbq_sched_data *q, struct cbq_class *cl)
{
	int toplevel = q->toplevel;

	if (toplevel > cl->level && !(cl->q->flags&TCQ_F_THROTTLED)) {
		psched_time_t now;
		psched_tdiff_t incr;

		now = psched_get_time();
		incr = now - q->now_rt;
		now = q->now + incr;

		do {
			if (cl->undertime < now) {
				q->toplevel = cl->level;
				return;
			}
		} while ((cl=cl->borrow) != NULL && toplevel > cl->level);
	}
}

static int
cbq_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	int len = skb->len;
	int ret;
	struct cbq_class *cl = cbq_classify(skb, sch, &ret);

#ifdef CONFIG_NET_CLS_POLICE
	q->rx_class = cl;
#endif
	if (cl == NULL) {
		if (ret == NET_XMIT_BYPASS)
			sch->qstats.drops++;
		kfree_skb(skb);
		return ret;
	}

#ifdef CONFIG_NET_CLS_POLICE
	cl->q->__parent = sch;
#endif
	if ((ret = cl->q->enqueue(skb, cl->q)) == NET_XMIT_SUCCESS) {
		sch->q.qlen++;
		sch->bstats.packets++;
		sch->bstats.bytes+=len;
		cbq_mark_toplevel(q, cl);
		if (!cl->next_alive)
			cbq_activate_class(cl);
		return ret;
	}

	sch->qstats.drops++;
	cbq_mark_toplevel(q, cl);
	cl->qstats.drops++;
	return ret;
}

static int
cbq_requeue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *cl;
	int ret;

	if ((cl = q->tx_class) == NULL) {
		kfree_skb(skb);
		sch->qstats.drops++;
		return NET_XMIT_CN;
	}
	q->tx_class = NULL;

	cbq_mark_toplevel(q, cl);

#ifdef CONFIG_NET_CLS_POLICE
	q->rx_class = cl;
	cl->q->__parent = sch;
#endif
	if ((ret = cl->q->ops->requeue(skb, cl->q)) == 0) {
		sch->q.qlen++;
		sch->qstats.requeues++;
		if (!cl->next_alive)
			cbq_activate_class(cl);
		return 0;
	}
	sch->qstats.drops++;
	cl->qstats.drops++;
	return ret;
}

/* Overlimit actions */

/* TC_CBQ_OVL_CLASSIC: (default) penalize leaf class by adding offtime */

static void cbq_ovl_classic(struct cbq_class *cl)
{
	struct cbq_sched_data *q = qdisc_priv(cl->qdisc);
	psched_tdiff_t delay = cl->undertime - q->now;

	if (!cl->delayed) {
		delay += cl->offtime;

		/*
		   Class goes to sleep, so that it will have no
		   chance to work avgidle. Let's forgive it 8)

		   BTW cbq-2.0 has a crap in this
		   place, apparently they forgot to shift it by cl->ewma_log.
		 */
		if (cl->avgidle < 0)
			delay -= (-cl->avgidle) - ((-cl->avgidle) >> cl->ewma_log);
		if (cl->avgidle < cl->minidle)
			cl->avgidle = cl->minidle;
		if (delay <= 0)
			delay = 1;
		cl->undertime = q->now + delay;

		cl->xstats.overactions++;
		cl->delayed = 1;
	}
	if (q->wd_expires == 0 || q->wd_expires > delay)
		q->wd_expires = delay;

	/* Dirty work! We must schedule wakeups based on
	   real available rate, rather than leaf rate,
	   which may be tiny (even zero).
	 */
	if (q->toplevel == TC_CBQ_MAXLEVEL) {
		struct cbq_class *b;
		psched_tdiff_t base_delay = q->wd_expires;

		for (b = cl->borrow; b; b = b->borrow) {
			delay = b->undertime - q->now;
			if (delay < base_delay) {
				if (delay <= 0)
					delay = 1;
				base_delay = delay;
			}
		}

		q->wd_expires = base_delay;
	}
}

/* TC_CBQ_OVL_RCLASSIC: penalize by offtime classes in hierarchy, when
   they go overlimit
 */

static void cbq_ovl_rclassic(struct cbq_class *cl)
{
	struct cbq_sched_data *q = qdisc_priv(cl->qdisc);
	struct cbq_class *this = cl;

	do {
		if (cl->level > q->toplevel) {
			cl = NULL;
			break;
		}
	} while ((cl = cl->borrow) != NULL);

	if (cl == NULL)
		cl = this;
	cbq_ovl_classic(cl);
}

/* TC_CBQ_OVL_DELAY: delay until it will go to underlimit */

static void cbq_ovl_delay(struct cbq_class *cl)
{
	struct cbq_sched_data *q = qdisc_priv(cl->qdisc);
	psched_tdiff_t delay = cl->undertime - q->now;

	if (!cl->delayed) {
		psched_time_t sched = q->now;
		ktime_t expires;

		delay += cl->offtime;
		if (cl->avgidle < 0)
			delay -= (-cl->avgidle) - ((-cl->avgidle) >> cl->ewma_log);
		if (cl->avgidle < cl->minidle)
			cl->avgidle = cl->minidle;
		cl->undertime = q->now + delay;

		if (delay > 0) {
			sched += delay + cl->penalty;
			cl->penalized = sched;
			cl->cpriority = TC_CBQ_MAXPRIO;
			q->pmask |= (1<<TC_CBQ_MAXPRIO);

			expires = ktime_set(0, 0);
			expires = ktime_add_ns(expires, PSCHED_US2NS(sched));
			if (hrtimer_try_to_cancel(&q->delay_timer) &&
			    ktime_to_ns(ktime_sub(q->delay_timer.expires,
						  expires)) > 0)
				q->delay_timer.expires = expires;
			hrtimer_restart(&q->delay_timer);
			cl->delayed = 1;
			cl->xstats.overactions++;
			return;
		}
		delay = 1;
	}
	if (q->wd_expires == 0 || q->wd_expires > delay)
		q->wd_expires = delay;
}

/* TC_CBQ_OVL_LOWPRIO: penalize class by lowering its priority band */

static void cbq_ovl_lowprio(struct cbq_class *cl)
{
	struct cbq_sched_data *q = qdisc_priv(cl->qdisc);

	cl->penalized = q->now + cl->penalty;

	if (cl->cpriority != cl->priority2) {
		cl->cpriority = cl->priority2;
		q->pmask |= (1<<cl->cpriority);
		cl->xstats.overactions++;
	}
	cbq_ovl_classic(cl);
}

/* TC_CBQ_OVL_DROP: penalize class by dropping */

static void cbq_ovl_drop(struct cbq_class *cl)
{
	if (cl->q->ops->drop)
		if (cl->q->ops->drop(cl->q))
			cl->qdisc->q.qlen--;
	cl->xstats.overactions++;
	cbq_ovl_classic(cl);
}

static psched_tdiff_t cbq_undelay_prio(struct cbq_sched_data *q, int prio,
				       psched_time_t now)
{
	struct cbq_class *cl;
	struct cbq_class *cl_prev = q->active[prio];
	psched_time_t sched = now;

	if (cl_prev == NULL)
		return 0;

	do {
		cl = cl_prev->next_alive;
		if (now - cl->penalized > 0) {
			cl_prev->next_alive = cl->next_alive;
			cl->next_alive = NULL;
			cl->cpriority = cl->priority;
			cl->delayed = 0;
			cbq_activate_class(cl);

			if (cl == q->active[prio]) {
				q->active[prio] = cl_prev;
				if (cl == q->active[prio]) {
					q->active[prio] = NULL;
					return 0;
				}
			}

			cl = cl_prev->next_alive;
		} else if (sched - cl->penalized > 0)
			sched = cl->penalized;
	} while ((cl_prev = cl) != q->active[prio]);

	return sched - now;
}

static enum hrtimer_restart cbq_undelay(struct hrtimer *timer)
{
	struct cbq_sched_data *q = container_of(timer, struct cbq_sched_data,
						delay_timer);
	struct Qdisc *sch = q->watchdog.qdisc;
	psched_time_t now;
	psched_tdiff_t delay = 0;
	unsigned pmask;

	now = psched_get_time();

	pmask = q->pmask;
	q->pmask = 0;

	while (pmask) {
		int prio = ffz(~pmask);
		psched_tdiff_t tmp;

		pmask &= ~(1<<prio);

		tmp = cbq_undelay_prio(q, prio, now);
		if (tmp > 0) {
			q->pmask |= 1<<prio;
			if (tmp < delay || delay == 0)
				delay = tmp;
		}
	}

	if (delay) {
		ktime_t time;

		time = ktime_set(0, 0);
		time = ktime_add_ns(time, PSCHED_US2NS(now + delay));
		hrtimer_start(&q->delay_timer, time, HRTIMER_MODE_ABS);
	}

	sch->flags &= ~TCQ_F_THROTTLED;
	netif_schedule(sch->dev);
	return HRTIMER_NORESTART;
}


#ifdef CONFIG_NET_CLS_POLICE

static int cbq_reshape_fail(struct sk_buff *skb, struct Qdisc *child)
{
	int len = skb->len;
	struct Qdisc *sch = child->__parent;
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *cl = q->rx_class;

	q->rx_class = NULL;

	if (cl && (cl = cbq_reclassify(skb, cl)) != NULL) {

		cbq_mark_toplevel(q, cl);

		q->rx_class = cl;
		cl->q->__parent = sch;

		if (cl->q->enqueue(skb, cl->q) == 0) {
			sch->q.qlen++;
			sch->bstats.packets++;
			sch->bstats.bytes+=len;
			if (!cl->next_alive)
				cbq_activate_class(cl);
			return 0;
		}
		sch->qstats.drops++;
		return 0;
	}

	sch->qstats.drops++;
	return -1;
}
#endif

/*
   It is mission critical procedure.

   We "regenerate" toplevel cutoff, if transmitting class
   has backlog and it is not regulated. It is not part of
   original CBQ description, but looks more reasonable.
   Probably, it is wrong. This question needs further investigation.
*/

static __inline__ void
cbq_update_toplevel(struct cbq_sched_data *q, struct cbq_class *cl,
		    struct cbq_class *borrowed)
{
	if (cl && q->toplevel >= borrowed->level) {
		if (cl->q->q.qlen > 1) {
			do {
				if (borrowed->undertime == PSCHED_PASTPERFECT) {
					q->toplevel = borrowed->level;
					return;
				}
			} while ((borrowed=borrowed->borrow) != NULL);
		}
#if 0
	/* It is not necessary now. Uncommenting it
	   will save CPU cycles, but decrease fairness.
	 */
		q->toplevel = TC_CBQ_MAXLEVEL;
#endif
	}
}

static void
cbq_update(struct cbq_sched_data *q)
{
	struct cbq_class *this = q->tx_class;
	struct cbq_class *cl = this;
	int len = q->tx_len;

	q->tx_class = NULL;

	for ( ; cl; cl = cl->share) {
		long avgidle = cl->avgidle;
		long idle;

		cl->bstats.packets++;
		cl->bstats.bytes += len;

		/*
		   (now - last) is total time between packet right edges.
		   (last_pktlen/rate) is "virtual" busy time, so that

			 idle = (now - last) - last_pktlen/rate
		 */

		idle = q->now - cl->last;
		if ((unsigned long)idle > 128*1024*1024) {
			avgidle = cl->maxidle;
		} else {
			idle -= L2T(cl, len);

		/* true_avgidle := (1-W)*true_avgidle + W*idle,
		   where W=2^{-ewma_log}. But cl->avgidle is scaled:
		   cl->avgidle == true_avgidle/W,
		   hence:
		 */
			avgidle += idle - (avgidle>>cl->ewma_log);
		}

		if (avgidle <= 0) {
			/* Overlimit or at-limit */

			if (avgidle < cl->minidle)
				avgidle = cl->minidle;

			cl->avgidle = avgidle;

			/* Calculate expected time, when this class
			   will be allowed to send.
			   It will occur, when:
			   (1-W)*true_avgidle + W*delay = 0, i.e.
			   idle = (1/W - 1)*(-true_avgidle)
			   or
			   idle = (1 - W)*(-cl->avgidle);
			 */
			idle = (-avgidle) - ((-avgidle) >> cl->ewma_log);

			/*
			   That is not all.
			   To maintain the rate allocated to the class,
			   we add to undertime virtual clock,
			   necessary to complete transmitted packet.
			   (len/phys_bandwidth has been already passed
			   to the moment of cbq_update)
			 */

			idle -= L2T(&q->link, len);
			idle += L2T(cl, len);

			cl->undertime = q->now + idle;
		} else {
			/* Underlimit */

			cl->undertime = PSCHED_PASTPERFECT;
			if (avgidle > cl->maxidle)
				cl->avgidle = cl->maxidle;
			else
				cl->avgidle = avgidle;
		}
		cl->last = q->now;
	}

	cbq_update_toplevel(q, this, q->tx_borrowed);
}

static __inline__ struct cbq_class *
cbq_under_limit(struct cbq_class *cl)
{
	struct cbq_sched_data *q = qdisc_priv(cl->qdisc);
	struct cbq_class *this_cl = cl;

	if (cl->tparent == NULL)
		return cl;

	if (cl->undertime == PSCHED_PASTPERFECT || q->now >= cl->undertime) {
		cl->delayed = 0;
		return cl;
	}

	do {
		/* It is very suspicious place. Now overlimit
		   action is generated for not bounded classes
		   only if link is completely congested.
		   Though it is in agree with ancestor-only paradigm,
		   it looks very stupid. Particularly,
		   it means that this chunk of code will either
		   never be called or result in strong amplification
		   of burstiness. Dangerous, silly, and, however,
		   no another solution exists.
		 */
		if ((cl = cl->borrow) == NULL) {
			this_cl->qstats.overlimits++;
			this_cl->overlimit(this_cl);
			return NULL;
		}
		if (cl->level > q->toplevel)
			return NULL;
	} while (cl->undertime != PSCHED_PASTPERFECT && q->now < cl->undertime);

	cl->delayed = 0;
	return cl;
}

static __inline__ struct sk_buff *
cbq_dequeue_prio(struct Qdisc *sch, int prio)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *cl_tail, *cl_prev, *cl;
	struct sk_buff *skb;
	int deficit;

	cl_tail = cl_prev = q->active[prio];
	cl = cl_prev->next_alive;

	do {
		deficit = 0;

		/* Start round */
		do {
			struct cbq_class *borrow = cl;

			if (cl->q->q.qlen &&
			    (borrow = cbq_under_limit(cl)) == NULL)
				goto skip_class;

			if (cl->deficit <= 0) {
				/* Class exhausted its allotment per
				   this round. Switch to the next one.
				 */
				deficit = 1;
				cl->deficit += cl->quantum;
				goto next_class;
			}

			skb = cl->q->dequeue(cl->q);

			/* Class did not give us any skb :-(
			   It could occur even if cl->q->q.qlen != 0
			   f.e. if cl->q == "tbf"
			 */
			if (skb == NULL)
				goto skip_class;

			cl->deficit -= skb->len;
			q->tx_class = cl;
			q->tx_borrowed = borrow;
			if (borrow != cl) {
#ifndef CBQ_XSTATS_BORROWS_BYTES
				borrow->xstats.borrows++;
				cl->xstats.borrows++;
#else
				borrow->xstats.borrows += skb->len;
				cl->xstats.borrows += skb->len;
#endif
			}
			q->tx_len = skb->len;

			if (cl->deficit <= 0) {
				q->active[prio] = cl;
				cl = cl->next_alive;
				cl->deficit += cl->quantum;
			}
			return skb;

skip_class:
			if (cl->q->q.qlen == 0 || prio != cl->cpriority) {
				/* Class is empty or penalized.
				   Unlink it from active chain.
				 */
				cl_prev->next_alive = cl->next_alive;
				cl->next_alive = NULL;

				/* Did cl_tail point to it? */
				if (cl == cl_tail) {
					/* Repair it! */
					cl_tail = cl_prev;

					/* Was it the last class in this band? */
					if (cl == cl_tail) {
						/* Kill the band! */
						q->active[prio] = NULL;
						q->activemask &= ~(1<<prio);
						if (cl->q->q.qlen)
							cbq_activate_class(cl);
						return NULL;
					}

					q->active[prio] = cl_tail;
				}
				if (cl->q->q.qlen)
					cbq_activate_class(cl);

				cl = cl_prev;
			}

next_class:
			cl_prev = cl;
			cl = cl->next_alive;
		} while (cl_prev != cl_tail);
	} while (deficit);

	q->active[prio] = cl_prev;

	return NULL;
}

static __inline__ struct sk_buff *
cbq_dequeue_1(struct Qdisc *sch)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct sk_buff *skb;
	unsigned activemask;

	activemask = q->activemask&0xFF;
	while (activemask) {
		int prio = ffz(~activemask);
		activemask &= ~(1<<prio);
		skb = cbq_dequeue_prio(sch, prio);
		if (skb)
			return skb;
	}
	return NULL;
}

static struct sk_buff *
cbq_dequeue(struct Qdisc *sch)
{
	struct sk_buff *skb;
	struct cbq_sched_data *q = qdisc_priv(sch);
	psched_time_t now;
	psched_tdiff_t incr;

	now = psched_get_time();
	incr = now - q->now_rt;

	if (q->tx_class) {
		psched_tdiff_t incr2;
		/* Time integrator. We calculate EOS time
		   by adding expected packet transmission time.
		   If real time is greater, we warp artificial clock,
		   so that:

		   cbq_time = max(real_time, work);
		 */
		incr2 = L2T(&q->link, q->tx_len);
		q->now += incr2;
		cbq_update(q);
		if ((incr -= incr2) < 0)
			incr = 0;
	}
	q->now += incr;
	q->now_rt = now;

	for (;;) {
		q->wd_expires = 0;

		skb = cbq_dequeue_1(sch);
		if (skb) {
			sch->q.qlen--;
			sch->flags &= ~TCQ_F_THROTTLED;
			return skb;
		}

		/* All the classes are overlimit.

		   It is possible, if:

		   1. Scheduler is empty.
		   2. Toplevel cutoff inhibited borrowing.
		   3. Root class is overlimit.

		   Reset 2d and 3d conditions and retry.

		   Note, that NS and cbq-2.0 are buggy, peeking
		   an arbitrary class is appropriate for ancestor-only
		   sharing, but not for toplevel algorithm.

		   Our version is better, but slower, because it requires
		   two passes, but it is unavoidable with top-level sharing.
		*/

		if (q->toplevel == TC_CBQ_MAXLEVEL &&
		    q->link.undertime == PSCHED_PASTPERFECT)
			break;

		q->toplevel = TC_CBQ_MAXLEVEL;
		q->link.undertime = PSCHED_PASTPERFECT;
	}

	/* No packets in scheduler or nobody wants to give them to us :-(
	   Sigh... start watchdog timer in the last case. */

	if (sch->q.qlen) {
		sch->qstats.overlimits++;
		if (q->wd_expires)
			qdisc_watchdog_schedule(&q->watchdog,
						now + q->wd_expires);
	}
	return NULL;
}

/* CBQ class maintanance routines */

static void cbq_adjust_levels(struct cbq_class *this)
{
	if (this == NULL)
		return;

	do {
		int level = 0;
		struct cbq_class *cl;

		if ((cl = this->children) != NULL) {
			do {
				if (cl->level > level)
					level = cl->level;
			} while ((cl = cl->sibling) != this->children);
		}
		this->level = level+1;
	} while ((this = this->tparent) != NULL);
}

static void cbq_normalize_quanta(struct cbq_sched_data *q, int prio)
{
	struct cbq_class *cl;
	unsigned h;

	if (q->quanta[prio] == 0)
		return;

	for (h=0; h<16; h++) {
		for (cl = q->classes[h]; cl; cl = cl->next) {
			/* BUGGGG... Beware! This expression suffer of
			   arithmetic overflows!
			 */
			if (cl->priority == prio) {
				cl->quantum = (cl->weight*cl->allot*q->nclasses[prio])/
					q->quanta[prio];
			}
			if (cl->quantum <= 0 || cl->quantum>32*cl->qdisc->dev->mtu) {
				printk(KERN_WARNING "CBQ: class %08x has bad quantum==%ld, repaired.\n", cl->classid, cl->quantum);
				cl->quantum = cl->qdisc->dev->mtu/2 + 1;
			}
		}
	}
}

static void cbq_sync_defmap(struct cbq_class *cl)
{
	struct cbq_sched_data *q = qdisc_priv(cl->qdisc);
	struct cbq_class *split = cl->split;
	unsigned h;
	int i;

	if (split == NULL)
		return;

	for (i=0; i<=TC_PRIO_MAX; i++) {
		if (split->defaults[i] == cl && !(cl->defmap&(1<<i)))
			split->defaults[i] = NULL;
	}

	for (i=0; i<=TC_PRIO_MAX; i++) {
		int level = split->level;

		if (split->defaults[i])
			continue;

		for (h=0; h<16; h++) {
			struct cbq_class *c;

			for (c = q->classes[h]; c; c = c->next) {
				if (c->split == split && c->level < level &&
				    c->defmap&(1<<i)) {
					split->defaults[i] = c;
					level = c->level;
				}
			}
		}
	}
}

static void cbq_change_defmap(struct cbq_class *cl, u32 splitid, u32 def, u32 mask)
{
	struct cbq_class *split = NULL;

	if (splitid == 0) {
		if ((split = cl->split) == NULL)
			return;
		splitid = split->classid;
	}

	if (split == NULL || split->classid != splitid) {
		for (split = cl->tparent; split; split = split->tparent)
			if (split->classid == splitid)
				break;
	}

	if (split == NULL)
		return;

	if (cl->split != split) {
		cl->defmap = 0;
		cbq_sync_defmap(cl);
		cl->split = split;
		cl->defmap = def&mask;
	} else
		cl->defmap = (cl->defmap&~mask)|(def&mask);

	cbq_sync_defmap(cl);
}

static void cbq_unlink_class(struct cbq_class *this)
{
	struct cbq_class *cl, **clp;
	struct cbq_sched_data *q = qdisc_priv(this->qdisc);

	for (clp = &q->classes[cbq_hash(this->classid)]; (cl = *clp) != NULL; clp = &cl->next) {
		if (cl == this) {
			*clp = cl->next;
			cl->next = NULL;
			break;
		}
	}

	if (this->tparent) {
		clp=&this->sibling;
		cl = *clp;
		do {
			if (cl == this) {
				*clp = cl->sibling;
				break;
			}
			clp = &cl->sibling;
		} while ((cl = *clp) != this->sibling);

		if (this->tparent->children == this) {
			this->tparent->children = this->sibling;
			if (this->sibling == this)
				this->tparent->children = NULL;
		}
	} else {
		BUG_TRAP(this->sibling == this);
	}
}

static void cbq_link_class(struct cbq_class *this)
{
	struct cbq_sched_data *q = qdisc_priv(this->qdisc);
	unsigned h = cbq_hash(this->classid);
	struct cbq_class *parent = this->tparent;

	this->sibling = this;
	this->next = q->classes[h];
	q->classes[h] = this;

	if (parent == NULL)
		return;

	if (parent->children == NULL) {
		parent->children = this;
	} else {
		this->sibling = parent->children->sibling;
		parent->children->sibling = this;
	}
}

static unsigned int cbq_drop(struct Qdisc* sch)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *cl, *cl_head;
	int prio;
	unsigned int len;

	for (prio = TC_CBQ_MAXPRIO; prio >= 0; prio--) {
		if ((cl_head = q->active[prio]) == NULL)
			continue;

		cl = cl_head;
		do {
			if (cl->q->ops->drop && (len = cl->q->ops->drop(cl->q))) {
				sch->q.qlen--;
				if (!cl->q->q.qlen)
					cbq_deactivate_class(cl);
				return len;
			}
		} while ((cl = cl->next_alive) != cl_head);
	}
	return 0;
}

static void
cbq_reset(struct Qdisc* sch)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *cl;
	int prio;
	unsigned h;

	q->activemask = 0;
	q->pmask = 0;
	q->tx_class = NULL;
	q->tx_borrowed = NULL;
	qdisc_watchdog_cancel(&q->watchdog);
	hrtimer_cancel(&q->delay_timer);
	q->toplevel = TC_CBQ_MAXLEVEL;
	q->now = psched_get_time();
	q->now_rt = q->now;

	for (prio = 0; prio <= TC_CBQ_MAXPRIO; prio++)
		q->active[prio] = NULL;

	for (h = 0; h < 16; h++) {
		for (cl = q->classes[h]; cl; cl = cl->next) {
			qdisc_reset(cl->q);

			cl->next_alive = NULL;
			cl->undertime = PSCHED_PASTPERFECT;
			cl->avgidle = cl->maxidle;
			cl->deficit = cl->quantum;
			cl->cpriority = cl->priority;
		}
	}
	sch->q.qlen = 0;
}


static int cbq_set_lss(struct cbq_class *cl, struct tc_cbq_lssopt *lss)
{
	if (lss->change&TCF_CBQ_LSS_FLAGS) {
		cl->share = (lss->flags&TCF_CBQ_LSS_ISOLATED) ? NULL : cl->tparent;
		cl->borrow = (lss->flags&TCF_CBQ_LSS_BOUNDED) ? NULL : cl->tparent;
	}
	if (lss->change&TCF_CBQ_LSS_EWMA)
		cl->ewma_log = lss->ewma_log;
	if (lss->change&TCF_CBQ_LSS_AVPKT)
		cl->avpkt = lss->avpkt;
	if (lss->change&TCF_CBQ_LSS_MINIDLE)
		cl->minidle = -(long)lss->minidle;
	if (lss->change&TCF_CBQ_LSS_MAXIDLE) {
		cl->maxidle = lss->maxidle;
		cl->avgidle = lss->maxidle;
	}
	if (lss->change&TCF_CBQ_LSS_OFFTIME)
		cl->offtime = lss->offtime;
	return 0;
}

static void cbq_rmprio(struct cbq_sched_data *q, struct cbq_class *cl)
{
	q->nclasses[cl->priority]--;
	q->quanta[cl->priority] -= cl->weight;
	cbq_normalize_quanta(q, cl->priority);
}

static void cbq_addprio(struct cbq_sched_data *q, struct cbq_class *cl)
{
	q->nclasses[cl->priority]++;
	q->quanta[cl->priority] += cl->weight;
	cbq_normalize_quanta(q, cl->priority);
}

static int cbq_set_wrr(struct cbq_class *cl, struct tc_cbq_wrropt *wrr)
{
	struct cbq_sched_data *q = qdisc_priv(cl->qdisc);

	if (wrr->allot)
		cl->allot = wrr->allot;
	if (wrr->weight)
		cl->weight = wrr->weight;
	if (wrr->priority) {
		cl->priority = wrr->priority-1;
		cl->cpriority = cl->priority;
		if (cl->priority >= cl->priority2)
			cl->priority2 = TC_CBQ_MAXPRIO-1;
	}

	cbq_addprio(q, cl);
	return 0;
}

static int cbq_set_overlimit(struct cbq_class *cl, struct tc_cbq_ovl *ovl)
{
	switch (ovl->strategy) {
	case TC_CBQ_OVL_CLASSIC:
		cl->overlimit = cbq_ovl_classic;
		break;
	case TC_CBQ_OVL_DELAY:
		cl->overlimit = cbq_ovl_delay;
		break;
	case TC_CBQ_OVL_LOWPRIO:
		if (ovl->priority2-1 >= TC_CBQ_MAXPRIO ||
		    ovl->priority2-1 <= cl->priority)
			return -EINVAL;
		cl->priority2 = ovl->priority2-1;
		cl->overlimit = cbq_ovl_lowprio;
		break;
	case TC_CBQ_OVL_DROP:
		cl->overlimit = cbq_ovl_drop;
		break;
	case TC_CBQ_OVL_RCLASSIC:
		cl->overlimit = cbq_ovl_rclassic;
		break;
	default:
		return -EINVAL;
	}
	cl->penalty = ovl->penalty;
	return 0;
}

#ifdef CONFIG_NET_CLS_POLICE
static int cbq_set_police(struct cbq_class *cl, struct tc_cbq_police *p)
{
	cl->police = p->police;

	if (cl->q->handle) {
		if (p->police == TC_POLICE_RECLASSIFY)
			cl->q->reshape_fail = cbq_reshape_fail;
		else
			cl->q->reshape_fail = NULL;
	}
	return 0;
}
#endif

static int cbq_set_fopt(struct cbq_class *cl, struct tc_cbq_fopt *fopt)
{
	cbq_change_defmap(cl, fopt->split, fopt->defmap, fopt->defchange);
	return 0;
}

static int cbq_init(struct Qdisc *sch, struct rtattr *opt)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct rtattr *tb[TCA_CBQ_MAX];
	struct tc_ratespec *r;

	if (rtattr_parse_nested(tb, TCA_CBQ_MAX, opt) < 0 ||
	    tb[TCA_CBQ_RTAB-1] == NULL || tb[TCA_CBQ_RATE-1] == NULL ||
	    RTA_PAYLOAD(tb[TCA_CBQ_RATE-1]) < sizeof(struct tc_ratespec))
		return -EINVAL;

	if (tb[TCA_CBQ_LSSOPT-1] &&
	    RTA_PAYLOAD(tb[TCA_CBQ_LSSOPT-1]) < sizeof(struct tc_cbq_lssopt))
		return -EINVAL;

	r = RTA_DATA(tb[TCA_CBQ_RATE-1]);

	if ((q->link.R_tab = qdisc_get_rtab(r, tb[TCA_CBQ_RTAB-1])) == NULL)
		return -EINVAL;

	q->link.refcnt = 1;
	q->link.sibling = &q->link;
	q->link.classid = sch->handle;
	q->link.qdisc = sch;
	if (!(q->link.q = qdisc_create_dflt(sch->dev, &pfifo_qdisc_ops,
					    sch->handle)))
		q->link.q = &noop_qdisc;

	q->link.priority = TC_CBQ_MAXPRIO-1;
	q->link.priority2 = TC_CBQ_MAXPRIO-1;
	q->link.cpriority = TC_CBQ_MAXPRIO-1;
	q->link.ovl_strategy = TC_CBQ_OVL_CLASSIC;
	q->link.overlimit = cbq_ovl_classic;
	q->link.allot = psched_mtu(sch->dev);
	q->link.quantum = q->link.allot;
	q->link.weight = q->link.R_tab->rate.rate;

	q->link.ewma_log = TC_CBQ_DEF_EWMA;
	q->link.avpkt = q->link.allot/2;
	q->link.minidle = -0x7FFFFFFF;
	q->link.stats_lock = &sch->dev->queue_lock;

	qdisc_watchdog_init(&q->watchdog, sch);
	hrtimer_init(&q->delay_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	q->delay_timer.function = cbq_undelay;
	q->toplevel = TC_CBQ_MAXLEVEL;
	q->now = psched_get_time();
	q->now_rt = q->now;

	cbq_link_class(&q->link);

	if (tb[TCA_CBQ_LSSOPT-1])
		cbq_set_lss(&q->link, RTA_DATA(tb[TCA_CBQ_LSSOPT-1]));

	cbq_addprio(q, &q->link);
	return 0;
}

static __inline__ int cbq_dump_rate(struct sk_buff *skb, struct cbq_class *cl)
{
	unsigned char *b = skb_tail_pointer(skb);

	RTA_PUT(skb, TCA_CBQ_RATE, sizeof(cl->R_tab->rate), &cl->R_tab->rate);
	return skb->len;

rtattr_failure:
	nlmsg_trim(skb, b);
	return -1;
}

static __inline__ int cbq_dump_lss(struct sk_buff *skb, struct cbq_class *cl)
{
	unsigned char *b = skb_tail_pointer(skb);
	struct tc_cbq_lssopt opt;

	opt.flags = 0;
	if (cl->borrow == NULL)
		opt.flags |= TCF_CBQ_LSS_BOUNDED;
	if (cl->share == NULL)
		opt.flags |= TCF_CBQ_LSS_ISOLATED;
	opt.ewma_log = cl->ewma_log;
	opt.level = cl->level;
	opt.avpkt = cl->avpkt;
	opt.maxidle = cl->maxidle;
	opt.minidle = (u32)(-cl->minidle);
	opt.offtime = cl->offtime;
	opt.change = ~0;
	RTA_PUT(skb, TCA_CBQ_LSSOPT, sizeof(opt), &opt);
	return skb->len;

rtattr_failure:
	nlmsg_trim(skb, b);
	return -1;
}

static __inline__ int cbq_dump_wrr(struct sk_buff *skb, struct cbq_class *cl)
{
	unsigned char *b = skb_tail_pointer(skb);
	struct tc_cbq_wrropt opt;

	opt.flags = 0;
	opt.allot = cl->allot;
	opt.priority = cl->priority+1;
	opt.cpriority = cl->cpriority+1;
	opt.weight = cl->weight;
	RTA_PUT(skb, TCA_CBQ_WRROPT, sizeof(opt), &opt);
	return skb->len;

rtattr_failure:
	nlmsg_trim(skb, b);
	return -1;
}

static __inline__ int cbq_dump_ovl(struct sk_buff *skb, struct cbq_class *cl)
{
	unsigned char *b = skb_tail_pointer(skb);
	struct tc_cbq_ovl opt;

	opt.strategy = cl->ovl_strategy;
	opt.priority2 = cl->priority2+1;
	opt.pad = 0;
	opt.penalty = cl->penalty;
	RTA_PUT(skb, TCA_CBQ_OVL_STRATEGY, sizeof(opt), &opt);
	return skb->len;

rtattr_failure:
	nlmsg_trim(skb, b);
	return -1;
}

static __inline__ int cbq_dump_fopt(struct sk_buff *skb, struct cbq_class *cl)
{
	unsigned char *b = skb_tail_pointer(skb);
	struct tc_cbq_fopt opt;

	if (cl->split || cl->defmap) {
		opt.split = cl->split ? cl->split->classid : 0;
		opt.defmap = cl->defmap;
		opt.defchange = ~0;
		RTA_PUT(skb, TCA_CBQ_FOPT, sizeof(opt), &opt);
	}
	return skb->len;

rtattr_failure:
	nlmsg_trim(skb, b);
	return -1;
}

#ifdef CONFIG_NET_CLS_POLICE
static __inline__ int cbq_dump_police(struct sk_buff *skb, struct cbq_class *cl)
{
	unsigned char *b = skb_tail_pointer(skb);
	struct tc_cbq_police opt;

	if (cl->police) {
		opt.police = cl->police;
		opt.__res1 = 0;
		opt.__res2 = 0;
		RTA_PUT(skb, TCA_CBQ_POLICE, sizeof(opt), &opt);
	}
	return skb->len;

rtattr_failure:
	nlmsg_trim(skb, b);
	return -1;
}
#endif

static int cbq_dump_attr(struct sk_buff *skb, struct cbq_class *cl)
{
	if (cbq_dump_lss(skb, cl) < 0 ||
	    cbq_dump_rate(skb, cl) < 0 ||
	    cbq_dump_wrr(skb, cl) < 0 ||
	    cbq_dump_ovl(skb, cl) < 0 ||
#ifdef CONFIG_NET_CLS_POLICE
	    cbq_dump_police(skb, cl) < 0 ||
#endif
	    cbq_dump_fopt(skb, cl) < 0)
		return -1;
	return 0;
}

static int cbq_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	unsigned char *b = skb_tail_pointer(skb);
	struct rtattr *rta;

	rta = (struct rtattr*)b;
	RTA_PUT(skb, TCA_OPTIONS, 0, NULL);
	if (cbq_dump_attr(skb, &q->link) < 0)
		goto rtattr_failure;
	rta->rta_len = skb_tail_pointer(skb) - b;
	return skb->len;

rtattr_failure:
	nlmsg_trim(skb, b);
	return -1;
}

static int
cbq_dump_stats(struct Qdisc *sch, struct gnet_dump *d)
{
	struct cbq_sched_data *q = qdisc_priv(sch);

	q->link.xstats.avgidle = q->link.avgidle;
	return gnet_stats_copy_app(d, &q->link.xstats, sizeof(q->link.xstats));
}

static int
cbq_dump_class(struct Qdisc *sch, unsigned long arg,
	       struct sk_buff *skb, struct tcmsg *tcm)
{
	struct cbq_class *cl = (struct cbq_class*)arg;
	unsigned char *b = skb_tail_pointer(skb);
	struct rtattr *rta;

	if (cl->tparent)
		tcm->tcm_parent = cl->tparent->classid;
	else
		tcm->tcm_parent = TC_H_ROOT;
	tcm->tcm_handle = cl->classid;
	tcm->tcm_info = cl->q->handle;

	rta = (struct rtattr*)b;
	RTA_PUT(skb, TCA_OPTIONS, 0, NULL);
	if (cbq_dump_attr(skb, cl) < 0)
		goto rtattr_failure;
	rta->rta_len = skb_tail_pointer(skb) - b;
	return skb->len;

rtattr_failure:
	nlmsg_trim(skb, b);
	return -1;
}

static int
cbq_dump_class_stats(struct Qdisc *sch, unsigned long arg,
	struct gnet_dump *d)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *cl = (struct cbq_class*)arg;

	cl->qstats.qlen = cl->q->q.qlen;
	cl->xstats.avgidle = cl->avgidle;
	cl->xstats.undertime = 0;

	if (cl->undertime != PSCHED_PASTPERFECT)
		cl->xstats.undertime = cl->undertime - q->now;

	if (gnet_stats_copy_basic(d, &cl->bstats) < 0 ||
#ifdef CONFIG_NET_ESTIMATOR
	    gnet_stats_copy_rate_est(d, &cl->rate_est) < 0 ||
#endif
	    gnet_stats_copy_queue(d, &cl->qstats) < 0)
		return -1;

	return gnet_stats_copy_app(d, &cl->xstats, sizeof(cl->xstats));
}

static int cbq_graft(struct Qdisc *sch, unsigned long arg, struct Qdisc *new,
		     struct Qdisc **old)
{
	struct cbq_class *cl = (struct cbq_class*)arg;

	if (cl) {
		if (new == NULL) {
			if ((new = qdisc_create_dflt(sch->dev, &pfifo_qdisc_ops,
						     cl->classid)) == NULL)
				return -ENOBUFS;
		} else {
#ifdef CONFIG_NET_CLS_POLICE
			if (cl->police == TC_POLICE_RECLASSIFY)
				new->reshape_fail = cbq_reshape_fail;
#endif
		}
		sch_tree_lock(sch);
		*old = xchg(&cl->q, new);
		qdisc_tree_decrease_qlen(*old, (*old)->q.qlen);
		qdisc_reset(*old);
		sch_tree_unlock(sch);

		return 0;
	}
	return -ENOENT;
}

static struct Qdisc *
cbq_leaf(struct Qdisc *sch, unsigned long arg)
{
	struct cbq_class *cl = (struct cbq_class*)arg;

	return cl ? cl->q : NULL;
}

static void cbq_qlen_notify(struct Qdisc *sch, unsigned long arg)
{
	struct cbq_class *cl = (struct cbq_class *)arg;

	if (cl->q->q.qlen == 0)
		cbq_deactivate_class(cl);
}

static unsigned long cbq_get(struct Qdisc *sch, u32 classid)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *cl = cbq_class_lookup(q, classid);

	if (cl) {
		cl->refcnt++;
		return (unsigned long)cl;
	}
	return 0;
}

static void cbq_destroy_class(struct Qdisc *sch, struct cbq_class *cl)
{
	struct cbq_sched_data *q = qdisc_priv(sch);

	BUG_TRAP(!cl->filters);

	tcf_destroy_chain(cl->filter_list);
	qdisc_destroy(cl->q);
	qdisc_put_rtab(cl->R_tab);
#ifdef CONFIG_NET_ESTIMATOR
	gen_kill_estimator(&cl->bstats, &cl->rate_est);
#endif
	if (cl != &q->link)
		kfree(cl);
}

static void
cbq_destroy(struct Qdisc* sch)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *cl;
	unsigned h;

#ifdef CONFIG_NET_CLS_POLICE
	q->rx_class = NULL;
#endif
	/*
	 * Filters must be destroyed first because we don't destroy the
	 * classes from root to leafs which means that filters can still
	 * be bound to classes which have been destroyed already. --TGR '04
	 */
	for (h = 0; h < 16; h++) {
		for (cl = q->classes[h]; cl; cl = cl->next) {
			tcf_destroy_chain(cl->filter_list);
			cl->filter_list = NULL;
		}
	}
	for (h = 0; h < 16; h++) {
		struct cbq_class *next;

		for (cl = q->classes[h]; cl; cl = next) {
			next = cl->next;
			cbq_destroy_class(sch, cl);
		}
	}
}

static void cbq_put(struct Qdisc *sch, unsigned long arg)
{
	struct cbq_class *cl = (struct cbq_class*)arg;

	if (--cl->refcnt == 0) {
#ifdef CONFIG_NET_CLS_POLICE
		struct cbq_sched_data *q = qdisc_priv(sch);

		spin_lock_bh(&sch->dev->queue_lock);
		if (q->rx_class == cl)
			q->rx_class = NULL;
		spin_unlock_bh(&sch->dev->queue_lock);
#endif

		cbq_destroy_class(sch, cl);
	}
}

static int
cbq_change_class(struct Qdisc *sch, u32 classid, u32 parentid, struct rtattr **tca,
		 unsigned long *arg)
{
	int err;
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *cl = (struct cbq_class*)*arg;
	struct rtattr *opt = tca[TCA_OPTIONS-1];
	struct rtattr *tb[TCA_CBQ_MAX];
	struct cbq_class *parent;
	struct qdisc_rate_table *rtab = NULL;

	if (opt==NULL || rtattr_parse_nested(tb, TCA_CBQ_MAX, opt))
		return -EINVAL;

	if (tb[TCA_CBQ_OVL_STRATEGY-1] &&
	    RTA_PAYLOAD(tb[TCA_CBQ_OVL_STRATEGY-1]) < sizeof(struct tc_cbq_ovl))
		return -EINVAL;

	if (tb[TCA_CBQ_FOPT-1] &&
	    RTA_PAYLOAD(tb[TCA_CBQ_FOPT-1]) < sizeof(struct tc_cbq_fopt))
		return -EINVAL;

	if (tb[TCA_CBQ_RATE-1] &&
	    RTA_PAYLOAD(tb[TCA_CBQ_RATE-1]) < sizeof(struct tc_ratespec))
			return -EINVAL;

	if (tb[TCA_CBQ_LSSOPT-1] &&
	    RTA_PAYLOAD(tb[TCA_CBQ_LSSOPT-1]) < sizeof(struct tc_cbq_lssopt))
			return -EINVAL;

	if (tb[TCA_CBQ_WRROPT-1] &&
	    RTA_PAYLOAD(tb[TCA_CBQ_WRROPT-1]) < sizeof(struct tc_cbq_wrropt))
			return -EINVAL;

#ifdef CONFIG_NET_CLS_POLICE
	if (tb[TCA_CBQ_POLICE-1] &&
	    RTA_PAYLOAD(tb[TCA_CBQ_POLICE-1]) < sizeof(struct tc_cbq_police))
			return -EINVAL;
#endif

	if (cl) {
		/* Check parent */
		if (parentid) {
			if (cl->tparent && cl->tparent->classid != parentid)
				return -EINVAL;
			if (!cl->tparent && parentid != TC_H_ROOT)
				return -EINVAL;
		}

		if (tb[TCA_CBQ_RATE-1]) {
			rtab = qdisc_get_rtab(RTA_DATA(tb[TCA_CBQ_RATE-1]), tb[TCA_CBQ_RTAB-1]);
			if (rtab == NULL)
				return -EINVAL;
		}

		/* Change class parameters */
		sch_tree_lock(sch);

		if (cl->next_alive != NULL)
			cbq_deactivate_class(cl);

		if (rtab) {
			rtab = xchg(&cl->R_tab, rtab);
			qdisc_put_rtab(rtab);
		}

		if (tb[TCA_CBQ_LSSOPT-1])
			cbq_set_lss(cl, RTA_DATA(tb[TCA_CBQ_LSSOPT-1]));

		if (tb[TCA_CBQ_WRROPT-1]) {
			cbq_rmprio(q, cl);
			cbq_set_wrr(cl, RTA_DATA(tb[TCA_CBQ_WRROPT-1]));
		}

		if (tb[TCA_CBQ_OVL_STRATEGY-1])
			cbq_set_overlimit(cl, RTA_DATA(tb[TCA_CBQ_OVL_STRATEGY-1]));

#ifdef CONFIG_NET_CLS_POLICE
		if (tb[TCA_CBQ_POLICE-1])
			cbq_set_police(cl, RTA_DATA(tb[TCA_CBQ_POLICE-1]));
#endif

		if (tb[TCA_CBQ_FOPT-1])
			cbq_set_fopt(cl, RTA_DATA(tb[TCA_CBQ_FOPT-1]));

		if (cl->q->q.qlen)
			cbq_activate_class(cl);

		sch_tree_unlock(sch);

#ifdef CONFIG_NET_ESTIMATOR
		if (tca[TCA_RATE-1])
			gen_replace_estimator(&cl->bstats, &cl->rate_est,
				cl->stats_lock, tca[TCA_RATE-1]);
#endif
		return 0;
	}

	if (parentid == TC_H_ROOT)
		return -EINVAL;

	if (tb[TCA_CBQ_WRROPT-1] == NULL || tb[TCA_CBQ_RATE-1] == NULL ||
	    tb[TCA_CBQ_LSSOPT-1] == NULL)
		return -EINVAL;

	rtab = qdisc_get_rtab(RTA_DATA(tb[TCA_CBQ_RATE-1]), tb[TCA_CBQ_RTAB-1]);
	if (rtab == NULL)
		return -EINVAL;

	if (classid) {
		err = -EINVAL;
		if (TC_H_MAJ(classid^sch->handle) || cbq_class_lookup(q, classid))
			goto failure;
	} else {
		int i;
		classid = TC_H_MAKE(sch->handle,0x8000);

		for (i=0; i<0x8000; i++) {
			if (++q->hgenerator >= 0x8000)
				q->hgenerator = 1;
			if (cbq_class_lookup(q, classid|q->hgenerator) == NULL)
				break;
		}
		err = -ENOSR;
		if (i >= 0x8000)
			goto failure;
		classid = classid|q->hgenerator;
	}

	parent = &q->link;
	if (parentid) {
		parent = cbq_class_lookup(q, parentid);
		err = -EINVAL;
		if (parent == NULL)
			goto failure;
	}

	err = -ENOBUFS;
	cl = kzalloc(sizeof(*cl), GFP_KERNEL);
	if (cl == NULL)
		goto failure;
	cl->R_tab = rtab;
	rtab = NULL;
	cl->refcnt = 1;
	if (!(cl->q = qdisc_create_dflt(sch->dev, &pfifo_qdisc_ops, classid)))
		cl->q = &noop_qdisc;
	cl->classid = classid;
	cl->tparent = parent;
	cl->qdisc = sch;
	cl->allot = parent->allot;
	cl->quantum = cl->allot;
	cl->weight = cl->R_tab->rate.rate;
	cl->stats_lock = &sch->dev->queue_lock;

	sch_tree_lock(sch);
	cbq_link_class(cl);
	cl->borrow = cl->tparent;
	if (cl->tparent != &q->link)
		cl->share = cl->tparent;
	cbq_adjust_levels(parent);
	cl->minidle = -0x7FFFFFFF;
	cbq_set_lss(cl, RTA_DATA(tb[TCA_CBQ_LSSOPT-1]));
	cbq_set_wrr(cl, RTA_DATA(tb[TCA_CBQ_WRROPT-1]));
	if (cl->ewma_log==0)
		cl->ewma_log = q->link.ewma_log;
	if (cl->maxidle==0)
		cl->maxidle = q->link.maxidle;
	if (cl->avpkt==0)
		cl->avpkt = q->link.avpkt;
	cl->overlimit = cbq_ovl_classic;
	if (tb[TCA_CBQ_OVL_STRATEGY-1])
		cbq_set_overlimit(cl, RTA_DATA(tb[TCA_CBQ_OVL_STRATEGY-1]));
#ifdef CONFIG_NET_CLS_POLICE
	if (tb[TCA_CBQ_POLICE-1])
		cbq_set_police(cl, RTA_DATA(tb[TCA_CBQ_POLICE-1]));
#endif
	if (tb[TCA_CBQ_FOPT-1])
		cbq_set_fopt(cl, RTA_DATA(tb[TCA_CBQ_FOPT-1]));
	sch_tree_unlock(sch);

#ifdef CONFIG_NET_ESTIMATOR
	if (tca[TCA_RATE-1])
		gen_new_estimator(&cl->bstats, &cl->rate_est,
			cl->stats_lock, tca[TCA_RATE-1]);
#endif

	*arg = (unsigned long)cl;
	return 0;

failure:
	qdisc_put_rtab(rtab);
	return err;
}

static int cbq_delete(struct Qdisc *sch, unsigned long arg)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *cl = (struct cbq_class*)arg;
	unsigned int qlen;

	if (cl->filters || cl->children || cl == &q->link)
		return -EBUSY;

	sch_tree_lock(sch);

	qlen = cl->q->q.qlen;
	qdisc_reset(cl->q);
	qdisc_tree_decrease_qlen(cl->q, qlen);

	if (cl->next_alive)
		cbq_deactivate_class(cl);

	if (q->tx_borrowed == cl)
		q->tx_borrowed = q->tx_class;
	if (q->tx_class == cl) {
		q->tx_class = NULL;
		q->tx_borrowed = NULL;
	}
#ifdef CONFIG_NET_CLS_POLICE
	if (q->rx_class == cl)
		q->rx_class = NULL;
#endif

	cbq_unlink_class(cl);
	cbq_adjust_levels(cl->tparent);
	cl->defmap = 0;
	cbq_sync_defmap(cl);

	cbq_rmprio(q, cl);
	sch_tree_unlock(sch);

	if (--cl->refcnt == 0)
		cbq_destroy_class(sch, cl);

	return 0;
}

static struct tcf_proto **cbq_find_tcf(struct Qdisc *sch, unsigned long arg)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *cl = (struct cbq_class *)arg;

	if (cl == NULL)
		cl = &q->link;

	return &cl->filter_list;
}

static unsigned long cbq_bind_filter(struct Qdisc *sch, unsigned long parent,
				     u32 classid)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	struct cbq_class *p = (struct cbq_class*)parent;
	struct cbq_class *cl = cbq_class_lookup(q, classid);

	if (cl) {
		if (p && p->level <= cl->level)
			return 0;
		cl->filters++;
		return (unsigned long)cl;
	}
	return 0;
}

static void cbq_unbind_filter(struct Qdisc *sch, unsigned long arg)
{
	struct cbq_class *cl = (struct cbq_class*)arg;

	cl->filters--;
}

static void cbq_walk(struct Qdisc *sch, struct qdisc_walker *arg)
{
	struct cbq_sched_data *q = qdisc_priv(sch);
	unsigned h;

	if (arg->stop)
		return;

	for (h = 0; h < 16; h++) {
		struct cbq_class *cl;

		for (cl = q->classes[h]; cl; cl = cl->next) {
			if (arg->count < arg->skip) {
				arg->count++;
				continue;
			}
			if (arg->fn(sch, (unsigned long)cl, arg) < 0) {
				arg->stop = 1;
				return;
			}
			arg->count++;
		}
	}
}

static struct Qdisc_class_ops cbq_class_ops = {
	.graft		=	cbq_graft,
	.leaf		=	cbq_leaf,
	.qlen_notify	=	cbq_qlen_notify,
	.get		=	cbq_get,
	.put		=	cbq_put,
	.change		=	cbq_change_class,
	.delete		=	cbq_delete,
	.walk		=	cbq_walk,
	.tcf_chain	=	cbq_find_tcf,
	.bind_tcf	=	cbq_bind_filter,
	.unbind_tcf	=	cbq_unbind_filter,
	.dump		=	cbq_dump_class,
	.dump_stats	=	cbq_dump_class_stats,
};

static struct Qdisc_ops cbq_qdisc_ops = {
	.next		=	NULL,
	.cl_ops		=	&cbq_class_ops,
	.id		=	"cbq",
	.priv_size	=	sizeof(struct cbq_sched_data),
	.enqueue	=	cbq_enqueue,
	.dequeue	=	cbq_dequeue,
	.requeue	=	cbq_requeue,
	.drop		=	cbq_drop,
	.init		=	cbq_init,
	.reset		=	cbq_reset,
	.destroy	=	cbq_destroy,
	.change		=	NULL,
	.dump		=	cbq_dump,
	.dump_stats	=	cbq_dump_stats,
	.owner		=	THIS_MODULE,
};

static int __init cbq_module_init(void)
{
	return register_qdisc(&cbq_qdisc_ops);
}
static void __exit cbq_module_exit(void)
{
	unregister_qdisc(&cbq_qdisc_ops);
}
module_init(cbq_module_init)
module_exit(cbq_module_exit)
MODULE_LICENSE("GPL");
