<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7, IE=EmulateIE10"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu5_3_1#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<style>
.apply_gen_wan{
 	text-align:center;
 	background-color:#4D595D;
 	width:99%;
 	margin-top:10px;
	border-radius: 0 0 3px 3px;
	-moz-border-radius-bottomright: 3px;
	-moz-border-radius-bottomleft: 3px;
	behavior: url(/PIE.htc);
	border-radius: 0 0 3px 3px;
}
.FormTable{
 	margin-top:10px;
}
</style>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
var wans_dualwan = '<% nvram_get("wans_dualwan"); %>';
var nowWAN = '<% get_parameter("flag"); %>';

if(dualWAN_support && ( wans_dualwan.search("wan") >= 0 || wans_dualwan.search("lan") >= 0)){
	var wan_type_name = wans_dualwan.split(" ")[<% nvram_get("wan_unit"); %>].toUpperCase();
	switch(wan_type_name){
		case "DSL":
			location.href = "Advanced_DSL_Content.asp";
			break;
		case "USB":
			location.href = "Advanced_Modem_Content.asp";
			break;
		default:
			break;
	}
}
<% login_state_hook(); %>
<% wan_get_parameter(); %>

/* DNSCRYPT-BEGIN */
// [Name, Fullname, DNSSEC]
<% get_resolver_array(); %>
/* DNSCRYPT-END */
/* STUBBY-BEGIN */
// [Name, Fullname, DNSSEC]
<% get_stubby_array(); %>
/* STUBBY-END */

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var original_wan_type = wan_proto;
var original_wan_dhcpenable = parseInt('<% nvram_get("wan_dhcpenable_x"); %>');
var original_dnsenable = parseInt('<% nvram_get("wan_dnsenable_x"); %>');
var wan_unit_flag = '<% nvram_get("wan_unit"); %>';
/* DNSCRYPT-BEGIN */
var dnscrypt_proxy_orig = '<% nvram_get("dnscrypt_proxy"); %>';
var dnscrypt_csv = '<% nvram_get("dnscrypt_csv"); %>';
/* DNSCRYPT-END */
/* STUBBY-BEGIN */
var stubby_proxy_orig = '<% nvram_get("stubby_proxy"); %>';
var stubby_csv = '<% nvram_get("stubby_csv"); %>';
var stubby_dns_value = '<% nvram_get("stubby_dns"); %>';
var accessindex = [];
var serverindex = [];
/* STUBBY-END */
var vpn_client1_adns = '<% nvram_get("vpn_client1_adns"); %>';
var vpn_client2_adns = '<% nvram_get("vpn_client2_adns"); %>';
var vpn_client1_state = '<% nvram_get("vpn_client1_state"); %>';
var vpn_client2_state = '<% nvram_get("vpn_client2_state"); %>';
var vpn_client1_enabled = '<% nvram_get("vpn_client1_enabled"); %>';
var vpn_client2_enabled = '<% nvram_get("vpn_client2_enabled"); %>';
var ipv6_enabled = ('<% nvram_get("ipv6_service"); %>' == "disabled") ? 0 : 1;
var machine_name = '<% get_machine_name(); %>';
var allow_routelocal = (((machine_name.search("arm") == -1) ? false : true) && ('<% nvram_get("allow_routelocal"); %>' ? true : false));

function initial(){
	if(!dualWAN_support){
		if(wan_unit_flag == 1){
			document.wanUnit_form.wan_unit.value = 0;
			document.wanUnit_form.target = "";
			document.wanUnit_form.submit();
		}
	}

	show_menu();
	change_wan_type(document.form.wan_proto.value, 0);
	fixed_change_wan_type(document.form.wan_proto.value);
	genWANSoption();
	//change_wan_unit(document.form.wan_unit);
	addOnlineHelp($("faq"), ["UPnP"]);
	change_wan_type(document.form.wan_proto.value, 0);

	if(document.form.wan_proto.value == "pppoe"
			|| document.form.wan_proto.value == "pptp"
			|| document.form.wan_proto.value == "l2tp"
			){
			document.form.wan_pppoe_username.value = decodeURIComponent('<% nvram_char_to_ascii("", "wan_pppoe_username"); %>');
			document.form.wan_pppoe_passwd.value = decodeURIComponent('<% nvram_char_to_ascii("", "wan_pppoe_passwd"); %>');
	}

	if (isSupport("dnssec")){
		document.getElementById("dnssec_tr").style.display = "";
		document.getElementById("dnssec_strict_span").style.display = "";
		document.form.dnssec_strict_ckb.checked = ('<% nvram_get("dnssec_check_unsigned_x"); %>' == 1) ? true : false;
/* STUBBY-BEGIN */
		document.getElementById("stubby_dnssec_span").style.display = "";
		document.form.stubby_dnssec_ckb.checked = ('<% nvram_get("dnssec_check_unsigned_x"); %>' == 1) ? true : false;
/* STUBBY-END */
	}

/* DNSCRYPT-BEGIN */
	if (isSupport("dnscrypt")){
		document.getElementById("dnscrypt_tr").style.display = "";
		display_resolver_opt();
		if (dnscrypt_csv != "/rom/dnscrypt-resolvers.csv")  // warn if not using rom csv file
			showhide("dnscrypt_csvwarn", true);
		else
			showhide("dnscrypt_csvwarn", false);
	}
	update_resolverlist();
/* DNSCRYPT-END */
/* STUBBY-BEGIN */
	if (isSupport("stubby")){
		document.getElementById("stubby_tr").style.display = "";
		display_resolver_opt();
		if (stubby_csv != "/rom/stubby-resolvers.csv")  // warn if not using rom csv file
			showhide("stubby_csvwarn", true);
		else
			showhide("stubby_csvwarn", false);
	}
	update_resolverlist();
	$("stubby_server").onmouseover = function (event) {
		event = event || window.event;
		var target = event.target ? event.target : event.srcElement;
		if ( target.nodeName.toLowerCase() === 'option' ) {
			stubby_details($(target).value);
		}
	}
/* STUBBY-END */
	display_upnp_range();
}

/* DNSCRYPT-BEGIN */
function update_resolverlist(){
// Resolver list
	free_options(document.form.dnscrypt1_resolver);
	currentresolver1 = "<% nvram_get("dnscrypt1_resolver"); %>";
	add_option(document.form.dnscrypt1_resolver, "Not Defined","none",(currentresolver1 == "none"));
	add_option(document.form.dnscrypt1_resolver, "Random","random",(currentresolver1 == "random"));

	free_options(document.form.dnscrypt2_resolver);
	currentresolver2 = "<% nvram_get("dnscrypt2_resolver"); %>";
	add_option(document.form.dnscrypt2_resolver, "Not Defined","none",(currentresolver2 == "none"));
	add_option(document.form.dnscrypt2_resolver, "Random","random",(currentresolver2 == "random"));

	var dnssec_enabled = document.form.dnssec_enable[0].checked;
	var dnscrypt_proxy = document.form.dnscrypt_proxy[0].checked;
	var dnscrypt_nologs = document.form.dnscrypt_nologs[0].checked;
	for(var i = 0; i < resolverarray.length; i++){
		// Exclude non-dnssec resolvers if dnssec is enabled
//		if ((dnssec_enabled == 1 && resolverarray[i][2] == "yes") || dnssec_enabled == 0) {
			if ((dnscrypt_nologs == 1 && resolverarray[i][3] == "yes") || dnscrypt_nologs == 0) {
				if ((resolverarray[i][0].indexOf('ipv6') != -1) && (ipv6_enabled != "disabled")) { //ipv6
					add_option(document.form.dnscrypt1_resolver,
						resolverarray[i][1] + (resolverarray[i][2] == "yes" ? " w/DNSSEC" : ""), resolverarray[i][0],
						(currentresolver1 == resolverarray[i][0]));
					add_option(document.form.dnscrypt2_resolver,
						resolverarray[i][1] + (resolverarray[i][2] == "yes" ? " w/DNSSEC" : ""), resolverarray[i][0],
						(currentresolver2 == resolverarray[i][0]));
				} else if (resolverarray[i][0].indexOf('ipv6') == -1) { //ipv4
					add_option(document.form.dnscrypt1_resolver,
						resolverarray[i][1] + (resolverarray[i][2] == "yes" ? " w/DNSSEC" : ""), resolverarray[i][0],
						(currentresolver1 == resolverarray[i][0]));
					add_option(document.form.dnscrypt2_resolver,
						resolverarray[i][1] + (resolverarray[i][2] == "yes" ? " w/DNSSEC" : ""), resolverarray[i][0],
						(currentresolver2 == resolverarray[i][0]));
				}
			}
//		}
	}
	display_resolver_opt();
}

function set_dnscrypt_protocol(instance, name){
	if (instance == 1) {
	if (name.indexOf('ipv6') == -1)
		document.form.dnscrypt1_ipv6.value = 0;
	else
		document.form.dnscrypt1_ipv6.value = 1;
	}
	if (instance == 2) {
	if (name.indexOf('ipv6') == -1)
		document.form.dnscrypt2_ipv6.value = 0;
	else
		document.form.dnscrypt2_ipv6.value = 1;
	}
}
function display_resolver_opt(){
	$("dnscrypt1_resolv_tr").style.display = (document.form.dnscrypt_proxy[0].checked) ? "" : "none";
	$("dnscrypt1_port_tr").style.display = (document.form.dnscrypt_proxy[0].checked) ? "" : "none";
	$("dnscrypt2_resolv_tr").style.display = (document.form.dnscrypt_proxy[0].checked) ? "" : "none";
	$("dnscrypt2_port_tr").style.display = (document.form.dnscrypt_proxy[0].checked) ? "" : "none";
	$("dnscrypt_log_tr").style.display = (document.form.dnscrypt_proxy[0].checked) ? "" : "none";
	$("dnscrypt_nologs_tr").style.display = (document.form.dnscrypt_proxy[0].checked) ? "" : "none";
	$("dnssec_strict_span").style.display = (document.form.dnssec_enable[0].checked) ? "" : "none";
}
/* DNSCRYPT-END */
/* STUBBY-BEGIN */
function update_resolverlist(){
// Server list
	free_options(document.form.stubby_server);

	var accessindexname = "<br>Selected servers:<br>";
	var currentserversarray = stubby_dns_value.split("&#60");
	var dnssec_enabled = document.form.dnssec_enable[0].checked;
	var stubby_dnssec = document.form.stubby_dnssec[0].checked;
	var stubby_proxy = document.form.stubby_proxy[0].checked;
	var stubby_nologs = document.form.stubby_nologs[0].checked;
	var stubby_noipv6 = document.form.stubby_noipv6[1].checked;
	accessindex = []; serverindex = [];  var k = 0;
	//	add_option(document.form.stubby_servers, "Not Defined","none",(currentservers == "none"));
	for(var i = 0; i < stubbyarray.length; i++){
		if ((dnssec_enabled == 1 && stubbyarray[i][7] == "yes") || dnssec_enabled == 0) {	// Exclude non-dnssec servers if dnssec is enabled
			if ((stubby_nologs == 1 && stubbyarray[i][8] == "yes") || stubby_nologs == 0) {	// Exclude logging servers
				if ((ipv6_enabled && stubbyarray[i][2].length > 0 && stubby_noipv6 == 0) || stubbyarray[i][1].length > 0) {	// Exclude ipv6 only servers if ipv6 not enabled
					// check if selected
					isselected = false;
					var searchip = "";
					for(var j = 1; j < currentserversarray.length; j++){
						searchip = "&#62" + stubbyarray[i][1] + "&#62" + stubbyarray[i][2];
						if ((currentserversarray[j].indexOf(searchip) >= 0) && (currentserversarray[j].indexOf("&#62"+stubbyarray[i][3]) >= 0)
						 && (stubbyarray[i][6].length > 0 ? currentserversarray[j].indexOf("&#62"+stubbyarray[i][6]) >= 0 : true)){	// use ip address,port and public key as lookup
							isselected = true;
							accessindex[j-1] = i;
							break;
						}
					}
					// add to selection list
					add_option(document.form.stubby_server,
						stubbyarray[i][0] + (stubbyarray[i][3] != "853" ? " (Port: " + stubbyarray[i][3] + ")" : ""), i, isselected);
					serverindex[k] = i; k++;
				}
			}
		}
	}
	for(var j = 0; j < accessindex.length; j++){
		if (accessindex[j] >= 0)
			accessindexname += stubbyarray[accessindex[j]][0] + (stubbyarray[accessindex[j]][3] != "853" ? " (Port: " + stubbyarray[accessindex[j]][3] + ")" : "") + ", ";
	}
	$("stubby_accessorder").innerHTML = accessindexname.substring(0, accessindexname.length-2);
	display_resolver_opt();
}

function update_accessorder(obj) {
	var accessindexname = "<br>Selected servers:<br>";
	for(var i = 0; i < document.form.stubby_server.length; i++){
		var currindex = accessindex.indexOf(serverindex[i]);
		if (document.form.stubby_server[i].selected){
			if (currindex < 0)
				accessindex[accessindex.length] = serverindex[i];	// add selection
		}
		else
			accessindex[currindex] = -1;	// mark as deleted
	}
	for(var j = 0; j < accessindex.length; j++){
		if (accessindex[j] >= 0)
			accessindexname += stubbyarray[accessindex[j]][0] + (stubbyarray[accessindex[j]][3] != "853" ? " (Port: " + stubbyarray[accessindex[j]][3] + ")" : "") + ", ";
	}
	$("stubby_accessorder").innerHTML = accessindexname.substring(0, accessindexname.length-2);
}

function is_ctrla(o,event){
	if(event.ctrlKey && event.keyCode == 65) {
		for(var i = 0; i < document.form.stubby_server.length; i++)
			document.form.stubby_server[i].selected = true;
		update_accessorder(o);
	}
	return true;
}

function display_resolver_opt(){
	$("stubby_port_tr").style.display = (document.form.stubby_proxy[0].checked) ? "" : "none";
	$("stubby_server_tr").style.display = (document.form.stubby_proxy[0].checked) ? "" : "none";
//	$("stubby_log_tr").style.display = (document.form.stubby_proxy[0].checked) ? "" : "none";
	$("stubby_nologs_tr").style.display = (document.form.stubby_proxy[0].checked) ? "" : "none";
	$("stubby_noipv6_tr").style.display = (document.form.stubby_proxy[0].checked && ipv6_enabled) ? "" : "none";
	$("stubby_ordered_tr").style.display = (document.form.stubby_proxy[0].checked) ? "" : "none";
//	$("stubby_accessorder").style.display = (document.form.stubby_access[1].checked) ? "" : "none";
	$("stubby_accessorder").style.display = (document.form.stubby_proxy[0].checked) ? "" : "none";
	$("dnssec_strict_span").style.display = (document.form.stubby_proxy[0].checked || document.form.dnssec_enable[1].checked) ? "none" : "";
	$("stubby_dnssec_tr").style.display = (document.form.stubby_proxy[0].checked && document.form.dnssec_enable[0].checked) ? "" : "none";
	$("stubby_dnssec_span").style.display = (document.form.stubby_proxy[0].checked) ? ((document.form.dnssec_enable[0].checked && document.form.stubby_dnssec[1].checked) ? "" : "none") : ((document.form.dnssec_enable[0].checked) ? "" : "none");
}

function stubby_details(i){
	var statusmenu = "";
	statusmenu += "<div class='StatusHint' style='width:300px;'>" + stubbyarray[i][0] + "</div>";
	statusmenu += "<span class='StatusHint'></span>";
	statusmenu += "Auth Domain: " + (stubbyarray[i][4].length ? stubbyarray[i][4] : "None") + "<br>";
	statusmenu += "IPv4 Addr: " + (stubbyarray[i][1].length ? stubbyarray[i][1] : "None") + "<br>";
	statusmenu += "IPv6 Addr: " + (stubbyarray[i][2].length ? stubbyarray[i][2] : "None") + "<br>";
	statusmenu += "Port: " + stubbyarray[i][3] + "<br>";

	return overlib(statusmenu, OFFSETX, 0, RIGHT, DELAY, 2000);
}
/* STUBBY-END */

function warn_dnssec_strict(){
if (!document.form.dnssec_strict_ckb.checked
/* STUBBY-BEGIN */
	|| !document.form.stubby_dnssec_ckb.checked
/* STUBBY-END */
	)
	alert("WARNING:\nDisabling Strict DNSSEC enforcement allows unsigned or misconfigured sites to be accepted as valid\nand should only be used for diagnostic purposes");
}

function display_upnp_range(){
	$("upnp_secure_mode").style.display = (document.form.wan_upnp_enable[0].checked) ? "" : "none";
	$("upnp_stun_enable").style.display = (document.form.wan_upnp_enable[0].checked) ? "" : "none";
	$("upnp_range_int").style.display = (document.form.wan_upnp_enable[0].checked) ? "" : "none";
	$("upnp_range_ext").style.display = (document.form.wan_upnp_enable[0].checked) ? "" : "none";
	$("upnp_flush_leases").style.display = (document.form.wan_upnp_enable[0].checked) ? "" : "none";
}

function change_wan_unit(obj){
	if(!dualWAN_support) return;

	if(obj.options[obj.selectedIndex].text == "DSL"){
		if(dsltmp_transmode == "atm")
			document.form.current_page.value = "Advanced_DSL_Content.asp";
		else //ptm
			document.form.current_page.value = "Advanced_VDSL_Content.asp";
	}else if(document.form.dsltmp_transmode){
		document.form.dsltmp_transmode.style.display = "none";
	}

	if(obj.options[obj.selectedIndex].text == "USB") {
		document.form.current_page.value = "Advanced_Modem_Content.asp";
//	}else if(obj.options[obj.selectedIndex].text == "WAN"|| obj.options[obj.selectedIndex].text == "Ethernet LAN"){
//		return false;
	}

	FormActions("apply.cgi", "change_wan_unit", "", "");
	document.form.target = "";
	document.form.submit();
}

function genWANSoption(){
	for(i=0; i<wans_dualwan.split(" ").length; i++){
		var wans_dualwan_NAME = wans_dualwan.split(" ")[i].toUpperCase();
		if(wans_dualwan_NAME == "LAN")
			wans_dualwan_NAME = "Ethernet LAN";
		document.form.wan_unit.options[i] = new Option(wans_dualwan_NAME, i);
	}
	document.form.wan_unit.selectedIndex = '<% nvram_get("wan_unit"); %>';

	if(wans_dualwan.search(" ") < 0 || wans_dualwan.split(" ")[1] == 'none' || !dualWAN_support)
		$("WANscap").style.display = "none";
}

function applyRule(){

	if(validForm()){
		inputCtrl(document.form.wan_dhcpenable_x[0], 1);
		inputCtrl(document.form.wan_dhcpenable_x[1], 1);
		if(!document.form.wan_dhcpenable_x[0].checked){
			inputCtrl(document.form.wan_ipaddr_x, 1);
			inputCtrl(document.form.wan_netmask_x, 1);
			inputCtrl(document.form.wan_gateway_x, 1);
		}

		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);
		if(!document.form.wan_dnsenable_x[0].checked){
			inputCtrl(document.form.wan_dns1_x, 1);
			inputCtrl(document.form.wan_dns2_x, 1);
		}

		document.form.action_script.value = "restart_wan_if";

/* DNSCRYPT-BEGIN */
		// Reset VPN Client DNSCrypt option when turning off DNSCrypt
		if(!document.form.dnscrypt_proxy[0].checked && (dnscrypt_proxy_orig == 1)) {
			if(vpn_client1_enabled == 1) {
				if(document.form.vpn_client1_adns.value == 4)
					document.form.vpn_client1_adns.value = 2;
			}
			if(vpn_client2_enabled == 1) {
				if(document.form.vpn_client2_adns.value == 4)
					document.form.vpn_client2_adns.value = 2;
			}
		}

		// Restart services if required
		//if(document.form.web_redirect.value != "<% nvram_get("web_redirect"); %>")
		//	document.form.action_script += ";restart_firewall";

		if((document.form.dnscrypt_proxy[0].checked != "<% nvram_get("dnscrypt_proxy"); %>") ||
		   (document.form.dnscrypt1_resolver.value != "<% nvram_get("dnscrypt1_resolver"); %>") ||
		   (document.form.dnscrypt1_port.value != "<% nvram_get("dnscrypt1_port"); %>") ||
		   (document.form.dnscrypt2_resolver.value != "<% nvram_get("dnscrypt2_resolver"); %>") ||
		   (document.form.dnscrypt2_port.value != "<% nvram_get("dnscrypt2_port"); %>") ||
		   (document.form.dnscrypt_log.value != "<% nvram_get("dnscrypt_log"); %>"))
			document.form.action_script.value += ";stop_dnscrypt";
/* DNSCRYPT-END */
/* STUBBY-BEGIN */

		// Reset VPN Client Stubby option when turning off Stubby
		if(!document.form.stubby_proxy[0].checked && (stubby_proxy_orig == 1)) {
			if(vpn_client1_enabled == 1) {
				if(document.form.vpn_client1_adns.value == 4)
					document.form.vpn_client1_adns.value = 2;
			}
			if(vpn_client2_enabled == 1) {
				if(document.form.vpn_client2_adns.value == 4)
					document.form.vpn_client2_adns.value = 2;
			}
		}

		// Restart services if required
		//if(document.form.web_redirect.value != "<% nvram_get("web_redirect"); %>")
		//	document.form.action_script += ";restart_firewall";

		if((document.form.stubby_proxy[0].checked != "<% nvram_get("stubby_proxy"); %>") ||
		   (document.form.stubby_dns.value != "<% nvram_get("stubby_dns"); %>") ||
		   (document.form.stubby_port.value != "<% nvram_get("stubby_port"); %>") ||
		   (document.form.stubby_loglevel.value != "<% nvram_get("stubby_loglevel"); %>"))
			document.form.action_script.value += ";stop_stubby";
/* STUBBY-END */

		document.form.action_wait.value = eval("<% get_default_reboot_time(); %> / 2");
		document.form.showloading_x.value = (document.form.showloading_x.value == "1") ? "0" : "1"; //force progress bar to always be shown
		showLoading();
		document.form.submit();
	}
}

// test if WAN IP & Gateway & DNS IP is a valid IP
// DNS IP allows to input nothing
function valid_IP(obj_name, obj_flag){
		// A : 1.0.0.0~126.255.255.255
		// B : 127.0.0.0~127.255.255.255 (forbidden)
		// C : 128.0.0.0~255.255.255.254
		var A_class_start = inet_network("1.0.0.0");
		var A_class_end = inet_network("126.255.255.255");
		var B_class_start = inet_network("127.0.0.0");
		var B_class_end = inet_network("127.255.255.255");
		var C_class_start = inet_network("128.0.0.0");
		var C_class_end = inet_network("255.255.255.255");

		var ip_obj = obj_name;
		var ip_num = inet_network(ip_obj.value);

		if(obj_flag == "DNS" && ip_num == -1){ //DNS allows to input nothing
			return true;
		}

		if(obj_flag == "GW" && ip_num == -1){ //GW allows to input nothing
			return true;
		}

		if(ip_num > A_class_start && ip_num < A_class_end)
			return true;
		else if(ip_num > B_class_start && ip_num < B_class_end){
			alert(ip_obj.value+" <#JS_validip#>");
			ip_obj.focus();
			ip_obj.select();
			return false;
		}
		else if(ip_num > C_class_start && ip_num < C_class_end)
			return true;
		else{
			alert(ip_obj.value+" <#JS_validip#>");
			ip_obj.focus();
			ip_obj.select();
			return false;
		}


}

function validForm(){
	if(!document.form.wan_dhcpenable_x[0].checked){// Set IP address by userself
		if(!valid_IP(document.form.wan_ipaddr_x, "")) return false;  //WAN IP
		if(!valid_IP(document.form.wan_gateway_x, "GW"))return false;  //Gateway IP

		//Viz hold on this subnet issue cuz WAN setting hashighest prio 2013.01
		/*//WAN IP conflict with LAN ip subnet
		if(matchSubnet2(document.form.wan_ipaddr_x.value, document.form.wan_netmask_x, document.form.lan_ipaddr.value, document.form.lan_netmask)){
				document.form.wan_ipaddr_x.focus();
				document.form.wan_ipaddr_x.select();
				alert("<#IPConnection_x_WAN_LAN_conflict#>");
				return false;
		}

		//Gateway IP conflict with LAN ip subnet
		var gateway_obj = document.form.wan_gateway_x;
		var gateway_num = inet_network(gateway_obj.value);
		if(gateway_num > 0 &&
		   matchSubnet2(document.form.wan_gateway_x.value, document.form.wan_netmask_x, document.form.lan_ipaddr.value, document.form.lan_netmask)){
				document.form.wan_gateway_x.focus();
				document.form.wan_gateway_x.select();
				alert("<#IPConnection_x_WAN_LAN_conflict#>");
				return false;
		}
		*/

		if(document.form.wan_gateway_x.value == document.form.wan_ipaddr_x.value){
			document.form.wan_ipaddr_x.focus();
			alert("<#IPConnection_warning_WANIPEQUALGatewayIP#>");
			return false;
		}

		// test if netmask is valid.
		var default_netmask = "";
		var wrong_netmask = 0;
		var netmask_obj = document.form.wan_netmask_x;
		var netmask_num = inet_network(netmask_obj.value);

		if(netmask_num==0){
			var netmask_reverse_num = 0;		//Viz 2011.07 : Let netmask 0.0.0.0 pass
		}else{
		var netmask_reverse_num = ~netmask_num;
		}

		if(netmask_num < 0) wrong_netmask = 1;

		var test_num = netmask_reverse_num;
		while(test_num != 0){
			if((test_num+1)%2 == 0)
				test_num = (test_num+1)/2-1;
			else{
				wrong_netmask = 1;
				break;
			}
		}
		if(wrong_netmask == 1){
			alert(netmask_obj.value+" <#JS_validip#>");
			netmask_obj.value = default_netmask;
			netmask_obj.focus();
			netmask_obj.select();
			return false;
		}
	}

	if(document.form.wan_dnsenable_x[1].checked == true && document.form.wan_proto.value != "dhcp" && document.form.wan_dns1_x.value == "" && document.form.wan_dns2_x.value == ""){
		document.form.wan_dns1_x.focus();
		alert("<#IPConnection_x_DNSServer_blank#>");
		return false;
	}

	if(!document.form.wan_dnsenable_x[0].checked){
		if(!valid_IP(document.form.wan_dns1_x, "DNS")) return false;  //DNS1
		if(!valid_IP(document.form.wan_dns2_x, "DNS")) return false;  //DNS2
	}

	if(document.form.wan_proto.value == "pppoe"
			|| document.form.wan_proto.value == "pptp"
			|| document.form.wan_proto.value == "l2tp"
			){
		if(!validate_string(document.form.wan_pppoe_username)
				|| !validate_string(document.form.wan_pppoe_passwd)
				)
			return false;

		if(!validate_number_range(document.form.wan_pppoe_idletime, 0, 4294967295))
			return false;
	}

	if(document.form.wan_proto.value == "pppoe"){
		if(!validate_number_range(document.form.wan_pppoe_mtu, 1280, 1500)
				|| !validate_number_range(document.form.wan_pppoe_mru, 1280, 1500))
			return false;

		if(document.form.wan_pppoe_mru.value > document.form.wan_pppoe_mtu.value){
//			alert("MRU must be less than or equal to MTU");
			document.form.wan_pppoe_mru.value = document.form.wan_pppoe_mtu.value;
//			document.form.wan_pppoe_mru.focus();
//			document.form.wan_pppoe_mru.select();
//			return false;
		}

		if(!validate_string(document.form.wan_pppoe_service)
				|| !validate_string(document.form.wan_pppoe_ac)
				|| !validate_string(document.form.wan_pppoe_hostuniq))
			return false;
	}

        if((document.form.wan_proto.value == "dhcp")
		|| (document.form.wan_proto.value == "static")){
			if(!validate_number_range(document.form.wan_mtu, 1280, 9000))
				return false;
	}

	if(document.form.wan_hostname.value.length > 0){
		var alert_str = validate_hostname(document.form.wan_hostname);

		if(alert_str != ""){
			showtext($("alert_msg1"), alert_str);
			$("alert_msg1").style.display = "";
			document.form.wan_hostname.focus();
			document.form.wan_hostname.select();
			return false;
		}else{
			$("alert_msg1").style.display = "none";
  	}

		document.form.wan_hostname.value = trim(document.form.wan_hostname.value);
	}

	if(document.form.wan_hwaddr_x.value.length > 0)
			if(!check_macaddr(document.form.wan_hwaddr_x,check_hwaddr_flag(document.form.wan_hwaddr_x))){
					document.form.wan_hwaddr_x.select();
					document.form.wan_hwaddr_x.focus();
		 	return false;
			}

	if (document.form.wan_upnp_enable[0].checked) {
		if((!validate_number_range(document.form.upnp_min_port_int, 1, 65535))
			|| (!validate_number_range(document.form.upnp_max_port_int, 1, 65535))
			|| (!validate_number_range(document.form.upnp_min_port_ext, 1, 65535))
			|| (!validate_number_range(document.form.upnp_max_port_ext, 1, 65535))) {
				return false;
		}
		if((parseInt(document.form.upnp_max_port_int.value) < parseInt(document.form.upnp_min_port_int.value))
			|| (parseInt(document.form.upnp_max_port_ext.value) < parseInt(document.form.upnp_min_port_ext.value))) {
				alert("Invalid UPNP ports!  First port must be lower than last port value.");
	                        return false;
		}
	}

	if(document.form.wan_heartbeat_x.value.length > 0)
		 if(!validate_string(document.form.wan_heartbeat_x))
		 	return false;

/* DNSCRYPT-BEGIN */
	if (document.form.dnscrypt_proxy[0].checked) {
		if ((document.form.dnscrypt1_resolver.value != "none") && (!validate_number_range(document.form.dnscrypt1_port, 1, 65535)))
			return false;
		if ((document.form.dnscrypt2_resolver.value != "none") && (!validate_number_range(document.form.dnscrypt2_port, 1, 65535)))
			return false;
	}

	if (document.form.dnscrypt_proxy[0].checked) {
		if ((document.form.dnscrypt1_resolver.value != "none") && (document.form.dnscrypt2_resolver.value != "none")) {
			if (document.form.dnscrypt1_port.value == document.form.dnscrypt2_port.value) {
				alert("DNSCrypt port conflict!  Both DNSCrypt resolvers cannot use the same port.");
				return false;
			}
			if (document.form.dnscrypt1_resolver.value == document.form.dnscrypt2_resolver.value) {
				alert("DNSCrypt resolver conflict!  Both DNSCrypt resolvers cannot be set to the same server.");
				return false;
			}
		}
	}
/* DNSCRYPT-END */
/* STUBBY-BEGIN */
	document.form.stubby_ipv4.value = 0;
	document.form.stubby_ipv6.value = 0;
	var stubby_dns_value = "";
	for (i=0; i < accessindex.length; i++) {
		currindex = accessindex[i];
		if (currindex >= 0) {
			stubby_dns_value += "<"+stubbyarray[currindex][0];
			for (j=1; j < stubbyarray[currindex].length; j++)
				stubby_dns_value += ">"+stubbyarray[currindex][j];
			if (stubbyarray[currindex][1].length > 0)
				document.form.stubby_ipv4.value++;	// number of ipv4 servers
			if (stubbyarray[currindex][2].length > 0)
				document.form.stubby_ipv6.value++;	// number of ipv6 servers
		}
	}
	document.form.stubby_dns.value = stubby_dns_value;	//save selected server info
	if (document.form.stubby_ipv4.value == 0 && document.form.stubby_ipv6.value == 0 && document.form.stubby_proxy[0].checked) {
		document.form.stubby_server.focus();
		alert("Must select at least one DoT server!");
		return false;
	}

	if (document.form.stubby_proxy[0].checked) {
		if (!validate_number_range(document.form.stubby_port, 1, 65535))
			return false;
	}
/* STUBBY-END */

	return true;
}

function done_validating(action){
	refreshpage();
}

function change_wan_type(wan_type, flag){
	if(typeof(flag) != "undefined")
		change_wan_dhcp_enable(flag);
	else
		change_wan_dhcp_enable(1);

	if(wan_type == "pppoe"){
		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);

		inputCtrl(document.form.wan_auth_x, 0);
		inputCtrl(document.form.wan_pppoe_username, 1);
		$('tr_pppoe_password').style.display = "";
		document.form.wan_pppoe_passwd.disabled = false;
		inputCtrl(document.form.wan_pppoe_idletime, 1);
		inputCtrl(document.form.wan_pppoe_idletime_check, 1);
		inputCtrl(document.form.wan_pppoe_mtu, 1);
		inputCtrl(document.form.wan_pppoe_mru, 1);
		inputCtrl(document.form.wan_pppoe_service, 1);
		inputCtrl(document.form.wan_pppoe_ac, 1);
		inputCtrl(document.form.wan_pppoe_hostuniq, 1);
		inputCtrl(document.form.dhcpc_mode, 0);
		inputCtrl(document.form.wan_mtu, 0);

		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.wan_pppoe_options_x, 1);
		inputCtrl(document.form.wan_pptp_options_x, 0);
		// 2008.03 James. patch for Oleg's patch. }
		$("vpn_server").style.display = "none";
		$("vpn_dhcp").style.display = "";
	}
	else if(wan_type == "pptp"){
		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);

		inputCtrl(document.form.wan_auth_x, 0);
		inputCtrl(document.form.wan_pppoe_username, 1);
		$('tr_pppoe_password').style.display = "";
		document.form.wan_pppoe_passwd.disabled = false;
		inputCtrl(document.form.wan_pppoe_idletime, 1);
		inputCtrl(document.form.wan_pppoe_idletime_check, 1);
		inputCtrl(document.form.wan_pppoe_mtu, 0);
		inputCtrl(document.form.wan_pppoe_mru, 0);
		inputCtrl(document.form.wan_pppoe_service, 0);
		inputCtrl(document.form.wan_pppoe_ac, 0);
		inputCtrl(document.form.wan_pppoe_hostuniq, 0);
		inputCtrl(document.form.dhcpc_mode, 0);
		inputCtrl(document.form.wan_mtu, 0);

		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.wan_pppoe_options_x, 1);
		inputCtrl(document.form.wan_pptp_options_x, 1);
		// 2008.03 James. patch for Oleg's patch. }
		$("vpn_server").style.display = "";
		$("vpn_dhcp").style.display = "none";
	}
	else if(wan_type == "l2tp"){
		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);

		inputCtrl(document.form.wan_auth_x, 0);
		inputCtrl(document.form.wan_pppoe_username, 1);
		$('tr_pppoe_password').style.display = "";
		document.form.wan_pppoe_passwd.disabled = false;
		inputCtrl(document.form.wan_pppoe_idletime, 0);
		inputCtrl(document.form.wan_pppoe_idletime_check, 0);
		inputCtrl(document.form.wan_pppoe_mtu, 0);
		inputCtrl(document.form.wan_pppoe_mru, 0);
		inputCtrl(document.form.wan_pppoe_service, 0);
		inputCtrl(document.form.wan_pppoe_ac, 0);
		inputCtrl(document.form.wan_pppoe_hostuniq, 0);
		inputCtrl(document.form.dhcpc_mode, 0);
		inputCtrl(document.form.wan_mtu, 0);

		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.wan_pppoe_options_x, 1);
		inputCtrl(document.form.wan_pptp_options_x, 0);
		// 2008.03 James. patch for Oleg's patch. }
		$("vpn_server").style.display = "";
		$("vpn_dhcp").style.display = "none";
	}
	else if(wan_type == "static"){
		inputCtrl(document.form.wan_dnsenable_x[0], 0);
		inputCtrl(document.form.wan_dnsenable_x[1], 0);

		inputCtrl(document.form.wan_auth_x, 1);
		inputCtrl(document.form.wan_pppoe_username, (document.form.wan_auth_x.value != ""));
		$('tr_pppoe_password').style.display = (document.form.wan_auth_x.value != "") ? "" : "none";
		document.form.wan_pppoe_passwd.disabled = (document.form.wan_auth_x.value != "") ? false : true;
		inputCtrl(document.form.wan_pppoe_idletime, 0);
		inputCtrl(document.form.wan_pppoe_idletime_check, 0);
		inputCtrl(document.form.wan_pppoe_mtu, 0);
		inputCtrl(document.form.wan_pppoe_mru, 0);
		inputCtrl(document.form.wan_pppoe_service, 0);
		inputCtrl(document.form.wan_pppoe_ac, 0);
		inputCtrl(document.form.wan_pppoe_hostuniq, 0);
		inputCtrl(document.form.dhcpc_mode, 0);
		inputCtrl(document.form.wan_mtu, 1);

		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.wan_pppoe_options_x, 0);
		inputCtrl(document.form.wan_pptp_options_x, 0);
		// 2008.03 James. patch for Oleg's patch. }
		$("vpn_server").style.display = "none";
		$("vpn_dhcp").style.display = "none";
	}
	else{	// Automatic IP or 802.11 MD or ""
		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);

		inputCtrl(document.form.wan_auth_x, 1);

		inputCtrl(document.form.wan_pppoe_username, (document.form.wan_auth_x.value != ""));
		$('tr_pppoe_password').style.display = (document.form.wan_auth_x.value != "") ? "" : "none";
		document.form.wan_pppoe_passwd.disabled = (document.form.wan_auth_x.value != "") ? false : true;

		inputCtrl(document.form.wan_pppoe_idletime, 0);
		inputCtrl(document.form.wan_pppoe_idletime_check, 0);
		inputCtrl(document.form.wan_pppoe_mtu, 0);
		inputCtrl(document.form.wan_pppoe_mru, 0);
		inputCtrl(document.form.wan_pppoe_service, 0);
		inputCtrl(document.form.wan_pppoe_ac, 0);
		inputCtrl(document.form.wan_pppoe_hostuniq, 0);
		inputCtrl(document.form.dhcpc_mode, 1);
		inputCtrl(document.form.wan_mtu, 1);

		// 2008.03 James. patch for Oleg's patch. {
		inputCtrl(document.form.wan_pppoe_options_x, 0);
		inputCtrl(document.form.wan_pptp_options_x, 0);
		// 2008.03 James. patch for Oleg's patch. }
		$("vpn_server").style.display = "none";
		$("vpn_dhcp").style.display = "none";
	}
}

function fixed_change_wan_type(wan_type){
	var flag = false;

	if(!document.form.wan_dhcpenable_x[0].checked){
		if(document.form.wan_ipaddr_x.value.length == 0)
			document.form.wan_ipaddr_x.focus();
		else if(document.form.wan_netmask_x.value.length == 0)
			document.form.wan_netmask_x.focus();
		else if(document.form.wan_gateway_x.value.length == 0)
			document.form.wan_gateway_x.focus();
		else
			flag = true;
	}else
		flag = true;

	if(wan_type == "pppoe"){
		if(wan_type == original_wan_type){
			document.form.wan_dnsenable_x[0].checked = original_dnsenable;
			document.form.wan_dnsenable_x[1].checked = !original_dnsenable;
			change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', original_dnsenable);
		}
		else{
			document.form.wan_dnsenable_x[0].checked = 1;
			document.form.wan_dnsenable_x[1].checked = 0;
			change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', 0);

			inputCtrl(document.form.wan_dns1_x, 0);
			inputCtrl(document.form.wan_dns2_x, 0);
		}
	}else if(wan_type == "pptp"	|| wan_type == "l2tp"){

		if(wan_type == original_wan_type){
			document.form.wan_dnsenable_x[0].checked = original_dnsenable;
			document.form.wan_dnsenable_x[1].checked = !original_dnsenable;
			change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', original_dnsenable);
		}
		else{
			document.form.wan_dnsenable_x[0].checked = 0;
			document.form.wan_dnsenable_x[1].checked = 1;
			change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', 0);

			inputCtrl(document.form.wan_dnsenable_x[0], 1);
			inputCtrl(document.form.wan_dnsenable_x[1], 1);
		}
	}
	else if(wan_type == "static"){
		document.form.wan_dnsenable_x[0].checked = 0;
		document.form.wan_dnsenable_x[1].checked = 1;
		document.form.wan_dnsenable_x[0].disabled = true;
		change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', 0);
	}
	else{	// wan_type == "dhcp"

		if(wan_type == original_wan_type){
			document.form.wan_dnsenable_x[0].checked = original_dnsenable;
			document.form.wan_dnsenable_x[1].checked = !original_dnsenable;
			change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', original_dnsenable);
		}
		else{
			document.form.wan_dnsenable_x[0].checked = 1;
			document.form.wan_dnsenable_x[1].checked = 0;
			change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', 0);

			inputCtrl(document.form.wan_dns1_x, 0);
			inputCtrl(document.form.wan_dns2_x, 0);
		}
	}

	if(wan_type != "static"){  // disable DNS server "Yes" when choosing static IP, Jieming add at 2012/12/18
		if(document.form.wan_dhcpenable_x[0].checked){
			inputCtrl(document.form.wan_dnsenable_x[0], 1);
			inputCtrl(document.form.wan_dnsenable_x[1], 1);
		}
		else{		//wan_dhcpenable_x NO
			document.form.wan_dnsenable_x[0].checked = 0;
			document.form.wan_dnsenable_x[1].checked = 1;

			inputCtrl(document.form.wan_dnsenable_x[0], 1);
			inputCtrl(document.form.wan_dnsenable_x[1], 1);
			document.form.wan_dnsenable_x[0].disabled = true;
		}
	}
}

function change_wan_dhcp_enable(flag){
	var wan_type = document.form.wan_proto.value;

	// 2008.03 James. patch for Oleg's patch. {
	if(wan_type == "pppoe"){
		if(flag == 1){
			if(wan_type == original_wan_type){
				document.form.wan_dhcpenable_x[0].checked = original_wan_dhcpenable;
				document.form.wan_dhcpenable_x[1].checked = !original_wan_dhcpenable;
			}
			else{
				document.form.wan_dhcpenable_x[0].checked = 1;
				document.form.wan_dhcpenable_x[1].checked = 0;
			}
		}

		$('IPsetting').style.display = "";
		inputCtrl(document.form.wan_dhcpenable_x[0], 1);
		inputCtrl(document.form.wan_dhcpenable_x[1], 1);

		var wan_dhcpenable = document.form.wan_dhcpenable_x[0].checked;

		inputCtrl(document.form.wan_ipaddr_x, !wan_dhcpenable);
		inputCtrl(document.form.wan_netmask_x, !wan_dhcpenable);
		inputCtrl(document.form.wan_gateway_x, !wan_dhcpenable);
	}
	// 2008.03 James. patch for Oleg's patch. }
	else if(wan_type == "pptp"
			|| wan_type == "l2tp"
			){
		if(flag == 1){
			if(wan_type == original_wan_type){
				document.form.wan_dhcpenable_x[0].checked = original_wan_dhcpenable;
				document.form.wan_dhcpenable_x[1].checked = !original_wan_dhcpenable;
			}
			else{
				document.form.wan_dhcpenable_x[0].checked = 0;
				document.form.wan_dhcpenable_x[1].checked = 1;
			}
		}

		$('IPsetting').style.display = "";
		inputCtrl(document.form.wan_dhcpenable_x[0], 1);
		inputCtrl(document.form.wan_dhcpenable_x[1], 1);

		var wan_dhcpenable = document.form.wan_dhcpenable_x[0].checked;

		inputCtrl(document.form.wan_ipaddr_x, !wan_dhcpenable);
		inputCtrl(document.form.wan_netmask_x, !wan_dhcpenable);
		inputCtrl(document.form.wan_gateway_x, !wan_dhcpenable);
	}
	else if(wan_type == "static"){
		document.form.wan_dhcpenable_x[0].checked = 0;
		document.form.wan_dhcpenable_x[1].checked = 1;

		inputCtrl(document.form.wan_dhcpenable_x[0], 0);
		inputCtrl(document.form.wan_dhcpenable_x[1], 0);

		$('IPsetting').style.display = "";
		inputCtrl(document.form.wan_ipaddr_x, 1);
		inputCtrl(document.form.wan_netmask_x, 1);
		inputCtrl(document.form.wan_gateway_x, 1);
	}
	else{	// wan_type == "dhcp"
		document.form.wan_dhcpenable_x[0].checked = 1;
		document.form.wan_dhcpenable_x[1].checked = 0;

		inputCtrl(document.form.wan_dhcpenable_x[0], 0);
		inputCtrl(document.form.wan_dhcpenable_x[1], 0);

		inputCtrl(document.form.wan_ipaddr_x, 0);
		inputCtrl(document.form.wan_netmask_x, 0);
		inputCtrl(document.form.wan_gateway_x, 0);
		$('IPsetting').style.display = "none";
	}

	if(document.form.wan_dhcpenable_x[0].checked){
		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);
	}
	else{		//wan_dhcpenable_x NO
		document.form.wan_dnsenable_x[0].checked = 0;
		document.form.wan_dnsenable_x[1].checked = 1;
		change_common_radio(document.form.wan_dnsenable_x, 'IPConnection', 'wan_dnsenable_x', 0);

		inputCtrl(document.form.wan_dnsenable_x[0], 1);
		inputCtrl(document.form.wan_dnsenable_x[1], 1);
		document.form.wan_dnsenable_x[0].disabled = true;
	}
}

function showMAC(){
	var tempMAC = "";
	document.form.wan_hwaddr_x.value = login_mac_str();
	document.form.wan_hwaddr_x.focus();
}

function check_macaddr(obj,flag){ //control hint of input mac address
	if(flag == 1){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		$("check_mac").innerHTML="<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";
		return false;
	}else if(flag ==2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		$("check_mac").innerHTML="<#IPConnection_x_illegal_mac#>";
		return false;
	}else{
		$("check_mac") ? $("check_mac").style.display="none" : true;
		return true;
	}
}

/* password item show or not */
function pass_checked(obj){
	switchType(obj, document.form.show_pass_1.checked, true);
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<script>
	if(sw_mode == 3){
		alert("<#page_not_support_mode_hint#>");
		location.href = "/index.asp";
	}
</script>
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="wanUnit_form" action="/apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_WAN_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WAN_Content.asp">
<input type="hidden" name="action_mode" value="change_wan_unit">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="wan_unit" value="">
</form>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="support_cdma" value="<% nvram_get("support_cdma"); %>">
<input type="hidden" name="current_page" value="Advanced_WAN_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">	<!-- set in applyRule -->
<input type="hidden" name="action_wait" value="60">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="showloading_x" value="<% nvram_get("showloading_x"); %>">
<input type="hidden" name="lan_ipaddr" value="<% nvram_get("lan_ipaddr"); %>" />
<input type="hidden" name="lan_netmask" value="<% nvram_get("lan_netmask"); %>" />
<input type="hidden" name="wan_pppoe_username_org" value="<% nvram_char_to_ascii("", "wan_pppoe_username"); %>" />
<input type="hidden" name="wan_pppoe_passwd_org" value="<% nvram_char_to_ascii("", "wan_pppoe_passwd"); %>" />
<input type="hidden" name="vpn_client1_adns" value="<% nvram_get("vpn_client1_adns"); %>" />
<input type="hidden" name="vpn_client2_adns" value="<% nvram_get("vpn_client2_adns"); %>" />
/* DNSCRYPT-BEGIN */
<input type="hidden" name="dnscrypt1_ipv6" value="<% nvram_get("dnscrypt1_ipv6"); %>" />
<input type="hidden" name="dnscrypt2_ipv6" value="<% nvram_get("dnscrypt2_ipv6"); %>" />
/* DNSCRYPT-END */
/* STUBBY-BEGIN */
<input type="hidden" name="stubby_dns" value="<% nvram_get("stubby_dns"); %>" />
<input type="hidden" name="stubby_ipv4" value="<% nvram_get("stubby_ipv4"); %>" />
<input type="hidden" name="stubby_ipv6" value="<% nvram_get("stubby_ipv6"); %>" />
/* STUBBY-END */
<input type="hidden" name="dnssec_check_unsigned_x" value="<% nvram_get("dnssec_check_unsigned_x"); %>" />

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>

	<td height="430" valign="top">
	  <div id="tabMenu" class="submenuBlock"></div>

	  <!--===================================Beginning of Main Content===========================================-->
	<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top">
			<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
				<tbody>
				<tr>
	  			<td bgcolor="#4D595D" valign="top">
		  			<div>&nbsp;</div>
		  			<div class="formfonttitle"><#menu5_3#> - <#menu5_3_1#></div>
		  			<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  			<div class="formfontdesc" style="margin-bottom:0px;"><#Layer3Forwarding_x_ConnectionType_sectiondesc#></div>

						<table id="WANscap" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
							<thead>
							<tr>
								<td colspan="2"><#wan_index#></td>
							</tr>
							</thead>
							<tr>
								<th><#wan_type#></th>
								<td align="left">
									<select class="input_option" name="wan_unit" onchange="change_wan_unit(this);"></select>
									<!--select id="dsltmp_transmode" name="dsltmp_transmode" class="input_option" style="margin-left:7px;" onChange="change_dsl_transmode(this);">
													<option value="atm" <% nvram_match("dsltmp_transmode", "atm", "selected"); %>>ADSL WAN (ATM)</option>
													<option value="ptm" <% nvram_match("dsltmp_transmode", "ptm", "selected"); %>>VDSL WAN (PTM)</option>
									</select-->
								</td>
							</tr>
						</table>

						<table id="t2BC" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
						  <thead>
						  <tr>
							<td colspan="2"><#t2BC#></td>
						  </tr>
						  </thead>

							<tr>
								<th><#Layer3Forwarding_x_ConnectionType_itemname#></th>
								<td align="left">
									<select id="wan_proto_menu" class="input_option" name="wan_proto" onchange="change_wan_type(this.value);fixed_change_wan_type(this.value);">
										<option value="dhcp" <% nvram_match("wan_proto", "dhcp", "selected"); %>><#BOP_ctype_title1#></option>
										<option value="static" <% nvram_match("wan_proto", "static", "selected"); %>><#BOP_ctype_title5#></option>
										<option value="pppoe" <% nvram_match("wan_proto", "pppoe", "selected"); %>>PPPoE</option>
										<option value="pptp" <% nvram_match("wan_proto", "pptp", "selected"); %>>PPTP</option>
										<option value="l2tp" <% nvram_match("wan_proto", "l2tp", "selected"); %>>L2TP</option>
									</select>
								</td>
							</tr>

							<tr>
								<th><#Enable_WAN#></th>
								<td>
									<input type="radio" name="wan_enable" class="input" value="1" <% nvram_match("wan_enable", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="wan_enable" class="input" value="0" <% nvram_match("wan_enable", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>

							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,22);"><#Enable_NAT#></a></th>
								<td>
									<input type="radio" name="wan_nat_x" class="input" value="1" <% nvram_match("wan_nat_x", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="wan_nat_x" class="input" value="0" <% nvram_match("wan_nat_x", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>
							<tr>
								<th>Redirect to error page</th>
								<td align="left">
									<select id="web_redirect" class="input_option" name="web_redirect" onchange="change_webaebn_type(this.value);fixed_change_wan_type(this.value);">
										<option value="0" <% nvram_match("web_redirect", "0", "selected"); %>>Never</option>
										<option value="1" <% nvram_match("web_redirect", "1", "selected"); %>>When Link down</option>
										<option value="2" <% nvram_match("web_redirect", "2", "selected"); %>>When WAN down</option>
										<option value="3" <% nvram_match("web_redirect", "3", "selected"); %>>Link or WAN down</option>
									</select>
								</td>
							</tr>
							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,23);"><#BasicConfig_EnableMediaServer_itemname#></a>&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp<a id="faq" href="" target="_blank" style="font-family:Lucida Console;text-decoration:underline;">UPnP&nbspFAQ</a></th>
								<td>
									<input type="radio" name="wan_upnp_enable" class="input" value="1" onclick="display_upnp_range();return change_common_radio(this, 'LANHostConfig', 'wan_upnp_enable', '1')" <% nvram_match("wan_upnp_enable", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="wan_upnp_enable" class="input" value="0" onclick="display_upnp_range();return change_common_radio(this, 'LANHostConfig', 'wan_upnp_enable', '0')" <% nvram_match("wan_upnp_enable", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>
							<tr id="upnp_secure_mode">
								<th>UPNP: Enable secure mode</th>
									<td>
										<input type="radio" name="upnp_secure" class="input" value="1" <% nvram_match_x("", "upnp_secure", "1", "checked"); %>><#checkbox_Yes#>
										<input type="radio" name="upnp_secure" class="input" value="0" <% nvram_match_x("", "upnp_secure", "0", "checked"); %>><#checkbox_No#>
									</td>
							</tr>
							<tr id="upnp_stun_enable">
								<th>UPNP: Use STUN for external address</th>
									<td>
										<input type="radio" name="upnp_stun" class="input" value="1" <% nvram_match_x("", "upnp_stun", "1", "checked"); %>><#checkbox_Yes#>
										<input type="radio" name="upnp_stun" class="input" value="0" <% nvram_match_x("", "upnp_stun", "0", "checked"); %>><#checkbox_No#>
									</td>
							</tr>
							<tr id="upnp_range_int">
								<th>UPNP: Allowed internal port range</th>
									<td>
										<input type="text" maxlength="5" name="upnp_min_port_int" class="input_6_table" value="<% nvram_get("upnp_min_port_int"); %>" onkeypress="return is_number(this,event);">
											to
										<input type="text" maxlength="5" name="upnp_max_port_int" class="input_6_table" value="<% nvram_get("upnp_max_port_int"); %>" onkeypress="return is_number(this,event);">
									</td>
							</tr>
							<tr id="upnp_range_ext">

								<th>UPNP: Allowed external port range</th>
									<td>
										<input type="text" maxlength="5" name="upnp_min_port_ext" class="input_6_table" value="<% nvram_get("upnp_min_port_ext"); %>" onkeypress="return is_number(this,event);">
											to
										<input type="text" maxlength="5" name="upnp_max_port_ext" class="input_6_table" value="<% nvram_get("upnp_max_port_ext"); %>" onkeypress="return is_number(this,event);">
									</td>
							</tr>
							<tr id="upnp_flush_leases">

								<th>UPNP: Clear existing leases</th>
									<td>
										<input type="radio" name="upnp_flush" class="input" value="1" <% nvram_match_x("", "upnp_flush", "1", "checked"); %>><#checkbox_Yes#>
										<input type="radio" name="upnp_flush" class="input" value="0" <% nvram_match_x("", "upnp_flush", "0", "checked"); %>><#checkbox_No#>
									</td>
							</tr>
						</table>

						<table id="IPsetting" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
							<thead>
							<tr>
								<td colspan="2"><#IPConnection_ExternalIPAddress_sectionname#></td>
							</tr>
							</thead>

							<tr>
								<th><#Layer3Forwarding_x_DHCPClient_itemname#></th>
								<td>
									<input type="radio" name="wan_dhcpenable_x" class="input" value="1" onclick="change_wan_dhcp_enable(0);" <% nvram_match("wan_dhcpenable_x", "1", "checked"); %>><#checkbox_Yes#>
									<input type="radio" name="wan_dhcpenable_x" class="input" value="0" onclick="change_wan_dhcp_enable(0);" <% nvram_match("wan_dhcpenable_x", "0", "checked"); %>><#checkbox_No#>
								</td>
							</tr>

							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,1);"><#IPConnection_ExternalIPAddress_itemname#></a></th>
								<td><input type="text" name="wan_ipaddr_x" maxlength="15" class="input_15_table" value="<% nvram_get("wan_ipaddr_x"); %>" onKeyPress="return is_ipaddr(this, event);" ></td>
							</tr>

							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,2);"><#IPConnection_x_ExternalSubnetMask_itemname#></a></th>
								<td><input type="text" name="wan_netmask_x" maxlength="15" class="input_15_table" value="<% nvram_get("wan_netmask_x"); %>" onKeyPress="return is_ipaddr(this, event);" ></td>
							</tr>

							<tr>
								<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,3);"><#IPConnection_x_ExternalGateway_itemname#></a></th>
								<td><input type="text" name="wan_gateway_x" maxlength="15" class="input_15_table" value="<% nvram_get("wan_gateway_x"); %>" onKeyPress="return is_ipaddr(this, event);" ></td>
							</tr>
						</table>

						<table id="DNSsetting" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
          		<thead>
            	<tr>
              <td colspan="2"><#IPConnection_x_DNSServerEnable_sectionname#></td>
            	</tr>
          		</thead>
         			<tr>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,12);"><#IPConnection_x_DNSServerEnable_itemname#></a></th>
								<td>
			  					<input type="radio" name="wan_dnsenable_x" class="input" value="1" onclick="return change_common_radio(this, 'IPConnection', 'wan_dnsenable_x', 1)" <% nvram_match("wan_dnsenable_x", "1", "checked"); %> /><#checkbox_Yes#>
			  					<input type="radio" name="wan_dnsenable_x" class="input" value="0" onclick="return change_common_radio(this, 'IPConnection', 'wan_dnsenable_x', 0)" <% nvram_match("wan_dnsenable_x", "0", "checked"); %> /><#checkbox_No#>
								</td>
          		</tr>

          		<tr>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,13);"><#IPConnection_x_DNSServer1_itemname#></a></th>
            		<td><input type="text" maxlength="15" class="input_15_table" name="wan_dns1_x" value="<% nvram_get("wan_dns1_x"); %>" onkeypress="return is_ipaddr(this, event)" ></td>
          		</tr>
          		<tr>
            		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,14);"><#IPConnection_x_DNSServer2_itemname#></a></th>
            		<td><input type="text" maxlength="15" class="input_15_table" name="wan_dns2_x" value="<% nvram_get("wan_dns2_x"); %>" onkeypress="return is_ipaddr(this, event)" ></td>
          		</tr>
/* DNSCRYPT-BEGIN */
			<tr id="dnscrypt_tr" style="display:none;">
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,33);">Enable DNSCRYPT support<br><i>DNSCRYPT becomes primary DNS server(s)</i></a></th>
				<td colspan="2" style="text-align:left;">
					<input type="radio" value="1" name="dnscrypt_proxy" onclick="display_resolver_opt();" <% nvram_match("dnscrypt_proxy", "1", "checked"); %> /><#checkbox_Yes#>
					<input type="radio" value="0" name="dnscrypt_proxy" onclick="display_resolver_opt();" <% nvram_match("dnscrypt_proxy", "0", "checked"); %> /><#checkbox_No#>
				<span id="dnscrypt_csvwarn" style="padding-left:42px;">Using updated resolver file</span>
				</td>
			</tr>
			<tr id="dnscrypt_nologs_tr" style="display:none;">
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,34);">Exclude DNSCRYPT servers with logs</a></th>
				<td colspan="2" style="text-align:left;">
					<input type="radio" value="1" name="dnscrypt_nologs" onclick="update_resolverlist();" <% nvram_match("dnscrypt_nologs", "1", "checked"); %> /><#checkbox_Yes#>
					<input type="radio" value="0" name="dnscrypt_nologs" onclick="update_resolverlist();" <% nvram_match("dnscrypt_nologs", "0", "checked"); %> /><#checkbox_No#>
				</td>
			</tr>
			<tr id="dnscrypt1_resolv_tr" style="display:none;">
				<th>DNSCRYPT Resolver1</th>
				<td colspan="2" style="text-align:left;">
					<select id="dnscrypt1_resolver" class="input_option" name="dnscrypt1_resolver" onclick="set_dnscrypt_protocol(1,this.value);">
						<option value="<% nvram_get("dnscrypt1_resolver"); %>" selected><% nvram_get("dnscrypt1_resolver"); %></option>
					</select>
				</td>
			</tr>
			<tr id="dnscrypt1_port_tr" style="display:none;">
				<th>DNSCRYPT Port1</th>
				<td colspan="2" style="text-align:left;">
					<input type="text" maxlength="5" name="dnscrypt1_port" class="input_6_table" value="<% nvram_get("dnscrypt1_port"); %>" onKeyPress="return is_number(this,event);"/>
				</td>
			</tr>
			<tr id="dnscrypt2_resolv_tr" style="display:none;">
				<th>DNSCRYPT Resolver2</th>
				<td colspan="2" style="text-align:left;">
					<select id="dnscrypt2_resolver" class="input_option" name="dnscrypt2_resolver" onclick="set_dnscrypt_protocol(2,this.value);">
						<option value="<% nvram_get("dnscrypt2_resolver"); %>" selected><% nvram_get("dnscrypt2_resolver"); %></option>
					</select>
				</td>
			</tr>
			<tr id="dnscrypt2_port_tr" style="display:none;">
				<th>DNSCRYPT Port2</th>
				<td colspan="2" style="text-align:left;">
					<input type="text" maxlength="5" name="dnscrypt2_port" class="input_6_table" value="<% nvram_get("dnscrypt2_port"); %>" onKeyPress="return is_number(this,event);"/>
				</td>
			</tr>
			<tr id="dnscrypt_log_tr" style="display:none;">
				<th>DNSCRYPT log level</th>
				<td colspan="2" style="text-align:left;">
					<select id="dnscrypt_log" class="input_option" name="dnscrypt_log">
						<option value="0" <% nvram_match("dnscrypt_log", "0", "selected"); %>>Emergency</option>
						<option value="1" <% nvram_match("dnscrypt_log", "1", "selected"); %>>Alert</option>
						<option value="2" <% nvram_match("dnscrypt_log", "2", "selected"); %>>Critical</option>
						<option value="3" <% nvram_match("dnscrypt_log", "3", "selected"); %>>Error</option>
						<option value="4" <% nvram_match("dnscrypt_log", "4", "selected"); %>>Warning</option>
						<option value="5" <% nvram_match("dnscrypt_log", "5", "selected"); %>>Notice</option>
						<option value="6" <% nvram_match("dnscrypt_log", "6", "selected"); %>>Info</option>
						<option value="7" <% nvram_match("dnscrypt_log", "7", "selected"); %>>Debug</option>
					</select>
				</td>
			</tr>
/* DNSCRYPT-END */
/* STUBBY-BEGIN */
			<tr id="stubby_tr" style="display:none;">
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,36);">Enable DNS over TLS support<br><i>DoT becomes primary DNS server(s)</i></a></th>
				<td colspan="2" style="text-align:left;">
					<input type="radio" value="1" name="stubby_proxy" onclick="display_resolver_opt();" <% nvram_match("stubby_proxy", "1", "checked"); %> /><#checkbox_Yes#>
					<input type="radio" value="0" name="stubby_proxy" onclick="display_resolver_opt();" <% nvram_match("stubby_proxy", "0", "checked"); %> /><#checkbox_No#>
				<span id="stubby_csvwarn" style="padding-left:42px;">Using updated server file</span>
				</td>
			</tr>
			<tr id="stubby_noipv6_tr" style="display:none;">
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,38);">Enable IPv6 DoT servers</a></th>
				<td colspan="2" style="text-align:left;">
					<input type="radio" value="0" name="stubby_noipv6" onclick="update_resolverlist();" <% nvram_match("stubby_noipv6", "0", "checked"); %> /><#checkbox_Yes#>
					<input type="radio" value="1" name="stubby_noipv6" onclick="update_resolverlist();" <% nvram_match("stubby_noipv6", "1", "checked"); %> /><#checkbox_No#>
				</td>
			</tr>
			<tr id="stubby_nologs_tr" style="display:none;">
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,37);">Exclude DoT servers with logs</a></th>
				<td colspan="2" style="text-align:left;">
					<input type="radio" value="1" name="stubby_nologs" onclick="update_resolverlist();" <% nvram_match("stubby_nologs", "1", "checked"); %> /><#checkbox_Yes#>
					<input type="radio" value="0" name="stubby_nologs" onclick="update_resolverlist();" <% nvram_match("stubby_nologs", "0", "checked"); %> /><#checkbox_No#>
				</td>
			</tr>
			<tr id="stubby_ordered_tr" style="display:none;">
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,39);">DoT server access</a></th>
				<td colspan="2" style="text-align:left;">
					<input type="radio" value="1" name="stubby_access" onclick="display_resolver_opt();" <% nvram_match("stubby_access", "1", "checked"); %> />RoundRobin
					<input type="radio" value="0" name="stubby_access" onclick="display_resolver_opt();" <% nvram_match("stubby_access", "0", "checked"); %> />Ordered
				</td>
			</tr>
			<tr id="stubby_server_tr" style="display:none;">
				<th>DoT Servers<br><i>Hold Ctrl / Cmd to select multiple servers<br>Hover on selection for server details</i></th>
				<td colspan="2" style="text-align:left;">
					<select id="stubby_server" name="stubby_server" class="multi_option" multiple size="5" onclick="update_accessorder(this);" onKeyDown="return is_ctrla(this, event);" onmouseout="nd();">
						<option value="none"></option>
					</select>
					<span id="stubby_accessorder" style="display:none;"><br>Selected servers</span>
				</td>
			</tr>
			<tr id="stubby_port_tr" style="display:none;">
				<th>DoT Local Port</th>
				<td colspan="2" style="text-align:left;">
					<input type="text" maxlength="5" name="stubby_port" class="input_6_table" value="<% nvram_get("stubby_port"); %>" onKeyPress="return is_number(this,event);"/>
				</td>
			</tr>
			<tr id="stubby_log_tr" style="display:none;">
				<th>DoT log level</th>
				<td colspan="2" style="text-align:left;">
					<select id="stubby_loglevel" class="input_option" name="stubby_loglevel">
						<option value="0" <% nvram_match("stubby_loglevel", "0", "selected"); %>>Emergency</option>
						<option value="1" <% nvram_match("stubby_loglevel", "1", "selected"); %>>Alert</option>
						<option value="2" <% nvram_match("stubby_loglevel", "2", "selected"); %>>Critical</option>
						<option value="3" <% nvram_match("stubby_loglevel", "3", "selected"); %>>Error</option>
						<option value="4" <% nvram_match("stubby_loglevel", "4", "selected"); %>>Warning</option>
						<option value="5" <% nvram_match("stubby_loglevel", "5", "selected"); %>>Notice</option>
						<option value="6" <% nvram_match("stubby_loglevel", "6", "selected"); %>>Info</option>
						<option value="7" <% nvram_match("stubby_loglevel", "7", "selected"); %>>Debug</option>
					</select>
				</td>
			</tr>
/* STUBBY-END */
			<tr id="dnssec_tr" style="display:none;">
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,32);">Enable DNSSEC support<br><i>DNS servers must support DNSSEC</i></a></th>
				<td colspan="2" style="text-align:left;">
					<input type="radio" value="1" name="dnssec_enable" onclick="display_resolver_opt();warn_dnssec_strict();" <% nvram_match("dnssec_enable", "1", "checked"); %> /><#checkbox_Yes#>
					<input type="radio" value="0" name="dnssec_enable" onclick="display_resolver_opt();" <% nvram_match("dnssec_enable", "0", "checked"); %> /><#checkbox_No#>
					<span id="dnssec_strict_span" style="display:none;padding-left:20px;"><input type="checkbox" name="dnssec_strict_ckb" id="dnssec_strict_ckb" value="<% nvram_get("dnssec_check_unsigned_x"); %>" onclick="document.form.dnssec_check_unsigned_x.value=(this.checked==true)?1:0;warn_dnssec_strict();"> Strict DNSSEC enforcement</input></span>
				</td>
			</tr>
/* STUBBY-BEGIN */
			<tr id="stubby_dnssec_tr" style="display:none;">
				<th>DNSSEC validation method</th>
				<td colspan="2" style="text-align:left;">
					<input type="radio" value="1" name="stubby_dnssec" onclick="display_resolver_opt();" <% nvram_match("stubby_dnssec", "1", "checked"); %> />GetDNS
					<input type="radio" value="0" name="stubby_dnssec" onclick="display_resolver_opt();" <% nvram_match("stubby_dnssec", "0", "checked"); %> />Dnsmasq
					<input type="radio" value="2" name="stubby_dnssec" onclick="display_resolver_opt();" <% nvram_match("stubby_dnssec", "2", "checked"); %> />Server Only
					<span id="stubby_dnssec_span" style="display:none;padding-left:20px;"><input type="checkbox" name="stubby_dnssec_ckb" id="stubby_dnssec_ckb" value="<% nvram_get("dnssec_check_unsigned_x"); %>" onclick="document.form.dnssec_check_unsigned_x.value=(this.checked==true)?1:0;warn_dnssec_strict();"> Strict DNSSEC enforcement</input></span>
				</td>
			</tr>
/* STUBBY-END */
			<tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,35);">Enable DNS Rebind protection</a></th>
				<td colspan="2" style="text-align:left;">
					<input type="radio" value="1" name="dns_norebind" <% nvram_match("dns_norebind", "1", "checked"); %> /><#checkbox_Yes#>
					<input type="radio" value="0" name="dns_norebind" <% nvram_match("dns_norebind", "0", "checked"); %> /><#checkbox_No#>
				</td>
			</tr>
			<tr id="dns_priv_override_tr">
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,40);">Prevent client auto DoH</a></th>
				<td colspan="2" style="text-align:left;">
					<input type="radio" value="0" name="dns_priv_override" <% nvram_match("dns_priv_override", "0", "checked"); %> />Auto
					<input type="radio" value="1" name="dns_priv_override" <% nvram_match("dns_priv_override", "1", "checked"); %> /><#checkbox_Yes#>
					<input type="radio" value="2" name="dns_priv_override" <% nvram_match("dns_priv_override", "2", "checked"); %> /><#checkbox_No#>
				</td>
			</tr>
        		</table>

		  			<table id="PPPsetting" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
            	<thead>
            	<tr>
              	<td colspan="2"><#PPPConnection_UserName_sectionname#></td>
            	</tr>
            	</thead>
                <th>MTU</th>
                <td><input type="text" maxlength="5" name="wan_mtu" class="input_6_table" value="<% nvram_get("wan_mtu"); %>" onKeyPress="return is_number(this,event);"/></td>
                </tr>
            	<tr>
							<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,29);"><#PPPConnection_Authentication_itemname#></a></th>
							<td align="left">
							    <select class="input_option" name="wan_auth_x" onChange="change_wan_type(document.form.wan_proto.value);">
							    <option value="" <% nvram_match("wan_auth_x", "", "selected"); %>><#wl_securitylevel_0#></option>
							    <option value="8021x-md5" <% nvram_match("wan_auth_x", "8021x-md5", "selected"); %>>802.1x MD5</option>
							    </select></td>
							</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,4);"><#PPPConnection_UserName_itemname#></a></th>
              	<td><input type="text" maxlength="64" class="input_32_table" name="wan_pppoe_username" value="<% nvram_get("wan_pppoe_username"); %>" onkeypress="return is_string(this, event)"></td>
            	</tr>
            	<tr id="tr_pppoe_password">
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,5);"><#PPPConnection_Password_itemname#></a></th>
              	<td>
					<div style="margin-top:2px;"><input type="password" readonly autocapitalization="off" maxlength="64" class="input_32_table" id="wan_pppoe_passwd" name="wan_pppoe_passwd" value="<% nvram_get("wan_pppoe_passwd"); %>" onFocus=" $(this).removeAttribute('readonly');"></div>
					<div style="margin-top:1px;"><input type="checkbox" name="show_pass_1" onclick="pass_checked(document.form.wan_pppoe_passwd);"><#QIS_show_pass#></div>
				</td>
            	</tr>
							<tr style="display:none">
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,6);"><#PPPConnection_IdleDisconnectTime_itemname#></a></th>
              	<td>
                	<input type="text" maxlength="10" class="input_12_table" name="wan_pppoe_idletime" value="<% nvram_get("wan_pppoe_idletime"); %>" onKeyPress="return is_number(this,event);" />
                	<input type="checkbox" style="margin-left:30;display:none;" name="wan_pppoe_idletime_check" value="" />
              	</td>
            	</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,7);"><#PPPConnection_x_PPPoEMTU_itemname#></a></th>
              	<td><input type="text" maxlength="5" name="wan_pppoe_mtu" class="input_6_table" value="<% nvram_get("wan_pppoe_mtu"); %>" onKeyPress="return is_number(this,event);"/></td>
            	</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,8);"><#PPPConnection_x_PPPoEMRU_itemname#></a></th>
              	<td><input type="text" maxlength="5" name="wan_pppoe_mru" class="input_6_table" value="<% nvram_get("wan_pppoe_mru"); %>" onKeyPress="return is_number(this,event);"/></td>
            	</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,9);"><#PPPConnection_x_ServiceName_itemname#></a></th>
              	<td><input type="text" maxlength="32" class="input_32_table" name="wan_pppoe_service" value="<% nvram_get("wan_pppoe_service"); %>" onkeypress="return is_string(this, event)"/></td>
            	</tr>
            	<tr>
              	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,10);"><#PPPConnection_x_AccessConcentrator_itemname#></a></th>
              	<td><input type="text" maxlength="32" class="input_32_table" name="wan_pppoe_ac" value="<% nvram_get("wan_pppoe_ac"); %>" onkeypress="return is_string(this, event)"/></td>
            	</tr>
		<tr>
		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,10);">Host Unique ID</a></th>
		<td><input type="text" maxlength="32" class="input_32_table" name="wan_pppoe_hostuniq" value="<% nvram_get("wan_pppoe_hostuniq"); %>" onkeypress="return is_string(this, event)"/></td>
		</tr>
            	<!-- 2008.03 James. patch for Oleg's patch. { -->
		<tr>
		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,17);"><#PPPConnection_x_PPTPOptions_itemname#></a></th>
		<td>
		<select name="wan_pptp_options_x" class="input_option">
			<option value="" <% nvram_match("wan_pptp_options_x", "","selected"); %>><#Auto#></option>
			<option value="-mppc" <% nvram_match("wan_pptp_options_x", "-mppc","selected"); %>><#No_Encryp#></option>
			<option value="+mppe-40" <% nvram_match("wan_pptp_options_x", "+mppe-40","selected"); %>>MPPE 40</option>
			<!--option value="+mppe-56" <% nvram_match("wan_pptp_options_x", "+mppe-56","selected"); %>>MPPE 56</option-->
			<option value="+mppe-128" <% nvram_match("wan_pptp_options_x", "+mppe-128","selected"); %>>MPPE 128</option>
		</select>
		</td>
		</tr>
		<tr>
		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,18);"><#PPPConnection_x_AdditionalOptions_itemname#></a></th>
		<td><input type="text" name="wan_pppoe_options_x" value="<% nvram_get("wan_pppoe_options_x"); %>" class="input_32_table" maxlength="255" onKeyPress="return is_string(this, event)" onBlur="validate_string(this)"></td>
		</tr>
		<!-- 2008.03 James. patch for Oleg's patch. } -->
          </table>

      <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
	  	<thead>
		<tr>
            	<td colspan="2"><#PPPConnection_x_HostNameForISP_sectionname#></td>
            	</tr>
		</thead>
		<tr id="vpn_server">
          	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,19);"><#BOP_isp_heart_item#></a></th>
          	<td>
          	<!-- 2008.03 James. patch for Oleg's patch. { -->
          	<input type="text" name="wan_heartbeat_x" class="input_32_table" maxlength="256" value="<% nvram_get("wan_heartbeat_x"); %>" onKeyPress="return is_string(this, event)"></td>
          	<!-- 2008.03 James. patch for Oleg's patch. } -->
        	</tr>
		<tr id="vpn_dhcp">
		<th><!--a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,);"-->Enable VPN + DHCP Connection<!--/a--></th>
		<td><input type="radio" name="wan_vpndhcp" class="input" value="1" onclick="return change_common_radio(this, 'IPConnection', 'wan_vpndhcp', 1)" <% nvram_match("wan_vpndhcp", "1", "checked"); %> /><#checkbox_Yes#>
		    <input type="radio" name="wan_vpndhcp" class="input" value="0" onclick="return change_common_radio(this, 'IPConnection', 'wan_vpndhcp', 0)" <% nvram_match("wan_vpndhcp", "0", "checked"); %> /><#checkbox_No#>
		</td>
        	</tr>
        	<tr>
          	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,15);"><#PPPConnection_x_HostNameForISP_itemname#></a></th>
          	<td>
          		<div><input type="text" name="wan_hostname" class="input_32_table" maxlength="32" value="<% nvram_get("wan_hostname"); %>" onkeypress="return is_string(this, event)"><br/><span id="alert_msg1" style="color:#FC0;"></span></div>
          	</td>
        	</tr>
        	<tr>
          	<th ><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,16);"><#PPPConnection_x_MacAddressForISP_itemname#></a></th>
				<td>
					<input type="text" name="wan_hwaddr_x" class="input_20_table" maxlength="17" value="<% nvram_get("wan_hwaddr_x"); %>" onKeyPress="return is_hwaddr(this,event)">
					<input type="button" class="button_gen_long" onclick="showMAC();" value="<#BOP_isp_MACclone#>">
				</td>
        	</tr>

        	<tr>
		<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(7,30);"><#DHCP_query_freq#></a></th>
        	<td>
        	<select name="dhcpc_mode" class="input_option">
			<option value="0" <% nvram_match(" dhcpc_mode", "0","selected"); %>><#DHCPnormal#></option>
			<option value="1" <% nvram_match(" dhcpc_mode", "1","selected"); %>><#DHCPaggressive#></option>
			<option value="2" <% nvram_match(" dhcpc_mode", "2","selected"); %>>Continuous Mode</option>
        	</select>
        	</td>
        	</tr>

                <tr>
                <th>Manual clientid (Option 61)</th>
                <td><input type="text" name="wan_dhcpc_options" class="input_32_table" maxlength="128" value="<% nvram_get("wan_dhcpc_options"); %>" onkeypress="return is_string(this, event)"></td>
                </tr>
		<tr>
                <th>Manual Vendor class (Option 60)</th>
                        <td><input type="text" name="wan_vendorid" class="input_32_table" maxlength="128" value="<% nvram_get("wan_vendorid"); %>" onkeypress="return is_string(this, event)"></td>
                </tr>
		</table>
	  <div class="apply_gen" style="height:auto">
			<input class="button_gen" onclick="applyRule();" type="button" value="<#CTL_apply#>"/>
	  </div>

                    </td>
                    </tr>

      	  </td>
      	  </tr>
</tbody>
</table>
</td>
</form>
				</tr>
			</table>

		</td>
		<!--===================================Ending of Main Content===========================================-->
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>

</body>
</html>
