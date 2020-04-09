/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

extern int ej_tcclass_dump_array(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_resolver_dump_array(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_stubby_dump_array(int eid, webs_t wp, int argc, char_t **argv);
extern int ej_get_custom_settings(int eid, webs_t wp, int argc, char **argv_);
extern void write_custom_settings(char *jstring);
extern int ej_get_addons_array(int eid, webs_t wp, int argc, char **argv_);
int tcclass_dump(FILE *fp, webs_t wp);
int resolver_dump(FILE *fp, webs_t wp);
int stubby_dump(FILE *fp, webs_t wp);
int parse_csv_line(char *line, long lineno);
