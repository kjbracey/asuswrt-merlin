<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<title><#Web_Title#> - DNS-based Filtering</title>
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<link rel="stylesheet" type="text/css" href="ParentalControl.css">
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link rel="stylesheet" type="text/css" href="/calendar/fullcalendar.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/client_function.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>

var $j = jQuery.noConflict();
var jData;
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
<% login_state_hook(); %>

var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var client_ip = login_ip_str();
var client_mac = login_mac_str();
var leases = [<% dhcp_leases(); %>];	// [[hostname, MAC, ip, lefttime], ...]
var arps = [<% get_arp_table(); %>];		// [[ip, x, x, MAC, x, type], ...]
var arls = [<% get_arl_table(); %>];		// [[MAC, port, x, x], ...]
var ipmonitor = [<% get_static_client(); %>];	// [[IP, MAC, DeviceName, Type, http, printer, iTune], ...]
var networkmap_fullscan = '<% nvram_match("networkmap_fullscan", "0", "done"); %>'; //2008.07.24 Add.  1 stands for complete, 0 stands for scanning.;
var clients_info = getclients();

var dnsfilter_rule_list = '<% nvram_get("dnsfilter_rulelist"); %>'.replace(/&#60/g, "<");
var dnsfilter_rule_list_row = dnsfilter_rule_list.split('<');


function initial(){
	show_menu();
	show_footer();

	if(!ParentalCtrl2_support){
		$('FormTitle').style.webkitBorderRadius = "3px";
		$('FormTitle').style.MozBorderRadius = "3px";
		$('FormTitle').style.BorderRadius = "3px";
	}

	gen_mainTable();
	showLANIPList();

	// dnsfilter_enable_x 0: disable, 1: enable, show mainTable & Protection level when enable, otherwise hide it
	showhide("dnsfilter_mode",document.form.dnsfilter_enable_x.value);
	showhide("dnsfilter_custom1", document.form.dnsfilter_enable_x.value);
	showhide("dnsfilter_custom2", document.form.dnsfilter_enable_x.value);
	showhide("dnsfilter_custom3", document.form.dnsfilter_enable_x.value);
	showhide("mainTable", document.form.dnsfilter_enable_x.value);
}

/*------------ Mouse event of fake LAN IP select menu {-----------------*/
function setClientIP(devname, macaddr){
	document.form.rule_devname.value = devname;
	document.form.rule_mac.value = macaddr;
	hideClients_Block();
	over_var = 0;
}

function showLANIPList(){
	var code = "";
	var show_name = "";
	var client_list_array = '<% get_client_detail_info(); %>';
	var client_list_row = client_list_array.split('<');

	for(var i = 1; i < client_list_row.length; i++){
		var client_list_col = client_list_row[i].split('>');
		if(client_list_col[1] && client_list_col[1].length > 20)
			show_name = client_list_col[1].substring(0, 16) + "..";
		else
			show_name = client_list_col[1];

		if(client_list_col[1])
			code += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\''+client_list_col[1]+'\', \''+client_list_col[3]+'\');"><strong>'+client_list_col[2]+'</strong> ';
		else {
			var macname = client_list_col[3].replace(/:/g,"-");
			code += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\''+macname+'\', \''+client_list_col[3]+'\');"><strong>'+client_list_col[2]+'</strong> ';
		}
		if(show_name && show_name.length > 0)
			code += '( '+show_name+')';
		code += ' </div></a>';
	}
	code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';
	$("ClientList_Block_PC").innerHTML = code;
}

function pullLANIPList(obj){
	if(isMenuopen == 0){
		obj.src = "/images/arrow-top.gif"
		$("ClientList_Block_PC").style.display = 'block';
		document.form.rule_devname.focus();
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}

var over_var = 0;
var isMenuopen = 0;

function hideClients_Block(){
	$("pull_arrow").src = "/images/arrow-down.gif";
	$('ClientList_Block_PC').style.display='none';
	isMenuopen = 0;
	//valid_IP_form(document.form.rule_devname, 0);
}

//see services.c for array mode assignments
function gen_modeselect(name, value, onchange){
	var code = "";
	code +='<select class="input_option" name="'+name+'" value="'+value+'" onchange="'+onchange+'">';
	code +='<option value="0"'+(value == 0 ? "selected" : "")+'>No Filtering</option>';
	code +='<option value="11"'+(value == 11 ? "selected" : "")+'>Router</option>';
	code +='<option value="1"'+(value == 1 ? "selected" : "")+'>OpenDNS Home</option>';
	code +='<option value="7"'+(value == 7 ? "selected" : "")+'>OpenDNS Family</option>';
//	code +='<option value="2"'+(value == 2 ? "selected" : "")+'>Norton Safe</option>';
//	code +='<option value="3"'+(value == 3 ? "selected" : "")+'>Norton Family</option>';
//	code +='<option value="4"'+(value == 4 ? "selected" : "")+'>Norton Children</option>';
	code +='<option value="19"'+(value == 19 ? "selected" : "")+'>Adguard Default</option>';
	code +='<option value="20"'+(value == 20 ? "selected" : "")+'>Adguard Family</option>';
	code +='<option value="5"'+(value == 5 ? "selected" : "")+'>Yandex Safe</option>';
	code +='<option value="6"'+(value == 6 ? "selected" : "")+'>Yandex Family</option>';
	code +='<option value="12"'+(value == 12? "selected" : "")+'>Comodo Secure DNS</option>';
	code +='<option value="17"'+(value == 17? "selected" : "")+'>Cloudflare Malware</option>';
	code +='<option value="18"'+(value == 18? "selected" : "")+'>Cloudflare Adult</option>';
	code +='<option value="13"'+(value == 13? "selected" : "")+'>Quad9</option>';
	code +='<option value="14"'+(value == 14? "selected" : "")+'>CleanBrowsing Security</option>';
	code +='<option value="15"'+(value == 15? "selected" : "")+'>CleanBrowsing Adult</option>';
	code +='<option value="16"'+(value == 16? "selected" : "")+'>CleanBrowsing Family</option>';
	code +='<option value="8"'+(value == 8 ? "selected" : "")+'>Custom 1</option>';
	code +='<option value="9"'+(value == 9 ? "selected" : "")+'>Custom 2</option>';
	code +='<option value="10"'+(value == 10 ? "selected" : "")+'>Custom 3</option>';
	code +='</select>';
	return code;
}

function gen_mainTable(){
	var code = "";

	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="FormTable_table" id="mainTable_table">';
	code +='<thead><tr><td colspan="4"><#ConnectedClient#>&nbsp;(<#List_limit#>&nbsp;64)</td></tr></thead>';
	code +='<tr><th width="40%"><#ParentalCtrl_username#></th>';
	code +='<th width="20%"><#ParentalCtrl_hwaddr#></th>';
	code +='<th width="15%">Filter Mode</th>';
	code +='<th width="10%"><#list_add_delete#></th></tr>';

	code +='<tr><td style="border-bottom:2px solid #000;"><input type="text" maxlength="32" style="margin-left:10px;float:left;width:255px;" class="input_20_table" name="rule_devname" onkeypress="return is_alphanum(this,event);" onClick="hideClients_Block();" onblur="if(!over_var){hideClients_Block();};is_safename(this)">';
	code +='<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" onclick="pullLANIPList(this);" title="<#select_client#>" onmouseover="over_var=1;" onmouseout="over_var=0;">';
	code +='<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div></td>';
	code +='<td style="border-bottom:2px solid #000;"><input type="text" maxlength="17" class="input_macaddr_table" name="rule_mac" onKeyPress="return is_hwaddr(this,event)"></td>';
	code +='<td style="border-bottom:2px solid #000;">'+gen_modeselect("rule_mode", "-1", "")+'</td>';
	code +='<td style="border-bottom:2px solid #000;"><input class="url_btn" type="button" onClick="addRow_main(64)" value=""></td></tr>';

	if(dnsfilter_rule_list_row == "")
		code +='<tr><td style="color:#FFCC00;" colspan="10"><#IPConnection_VSList_Norule#></td>';
	else{
		for(i=1; i<dnsfilter_rule_list_row.length; i++){
			var ruleArray = dnsfilter_rule_list_row[i].split('&#62');
			var rule_devname = ruleArray[0];
			var rule_mac = ruleArray[1];
			var rule_mode = ruleArray[2];

			code +='<tr id="row'+i+'">';
			code +='<td title="'+rule_devname+'">'+ rule_devname +'</td>';
			code +='<td title="'+rule_mac+'">'+ rule_mac +'</td>';
			code +='<td>'+gen_modeselect("rule_mode"+i, rule_mode, "changeRow_main(this);")+'</td>';
			code +='<td><input class=\"remove_btn\" type=\"button\" onclick=\"deleteRow_main(this);\" value=\"\"/></td>';
		}
	}

	code +='</tr></table>';

	$("mainTable").style.display = "";
	$("mainTable").innerHTML = code;
	$j("#mainTable").fadeIn();
	showLANIPList();
}

function applyRule(){
	var dnsfilter_rulelist_tmp = "";

	for(i=1; i<dnsfilter_rule_list_row.length; i++){
		dnsfilter_rulelist_tmp += "<";
		dnsfilter_rulelist_tmp += dnsfilter_rule_list_row[i].replace(/&#62/g, ">" );
	}
	document.form.dnsfilter_rulelist.value = dnsfilter_rulelist_tmp;

	showLoading();
	document.form.submit();
}

function check_macaddr(obj,flag){ //control hint of input mac address
	if(flag == 1){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		$("check_mac").innerHTML="<#LANHostConfig_ManualDHCPMacaddr_itemdesc#>";
		$("check_mac").style.display = "";
		return false;
	}else if(flag == 2){
		var childsel=document.createElement("div");
		childsel.setAttribute("id","check_mac");
		childsel.style.color="#FFCC00";
		obj.parentNode.appendChild(childsel);
		$("check_mac").innerHTML="<#IPConnection_x_illegal_mac#>";
		$("check_mac").style.display = "";
		return false;
	}else{
		$("check_mac") ? $("check_mac").style.display="none" : true;
		return true;
	}
}

function addRow_main(upper){
	var rule_num = $('mainTable_table').rows.length;
	var item_num = $('mainTable_table').rows[0].cells.length;

	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;
	}

	if(!validate_string(document.form.rule_devname))
		return false;

	if(document.form.rule_devname.value == ""){
		alert("<#JS_fieldblank#>");
		document.form.rule_devname.focus();
		return false;
	}

	if(document.form.rule_mac.value == ""){
		alert("<#JS_fieldblank#>");
		document.form.rule_mac.focus();
		return false;
	}

	if(!check_macaddr(document.form.rule_mac, check_hwaddr_flag(document.form.rule_mac))){
		document.form.rule_mac.focus();
		document.form.rule_mac.select();
		return false;
	}

	if(document.form.rule_mode.value == document.form.dnsfilter_mode.value){
		alert("Cannot specify a rule which matches the global filter mode!");
		document.form.rule_mode.focus();
		return false;
	}

	dnsfilter_rule_list += "<";
	dnsfilter_rule_list += document.form.rule_devname.value + "&#62";
	dnsfilter_rule_list += document.form.rule_mac.value + "&#62";
	dnsfilter_rule_list += document.form.rule_mode.value;

	dnsfilter_rule_list_row = dnsfilter_rule_list.split('<');

	gen_mainTable();
}

function deleteRow_main(r){
	var j=r.parentNode.parentNode.rowIndex;
	$(r.parentNode.parentNode.parentNode.parentNode.id).deleteRow(j);

	var dnsfilter_devname = "";
	var dnsfilter_mac = "";
	var dnsfilter_mode = "";
	var dnsfilter_rulelist_tmp = "";

	for(i=3; i<$('mainTable_table').rows.length; i++){
		dnsfilter_devname = $('mainTable_table').rows[i].cells[0].title;
		dnsfilter_mac = $('mainTable_table').rows[i].cells[1].title;
		dnsfilter_mode = $('mainTable_table').rows[i].cells[2].childNodes[0].value;

		dnsfilter_rulelist_tmp += "<";
		dnsfilter_rulelist_tmp += dnsfilter_devname + "&#62";
		dnsfilter_rulelist_tmp += dnsfilter_mac + "&#62";
		dnsfilter_rulelist_tmp += dnsfilter_mode;
	}

	dnsfilter_rule_list = dnsfilter_rulelist_tmp;
	dnsfilter_rule_list_row = dnsfilter_rulelist_tmp.split('<');

	gen_mainTable();
}

function changeRow_main(r){
	var dnsfilter_devname = "";
	var dnsfilter_mac = "";
	var dnsfilter_mode = "";
	var dnsfilter_rulelist_tmp = "";

	for(i=3; i<$('mainTable_table').rows.length; i++){
		dnsfilter_devname = $('mainTable_table').rows[i].cells[0].title;
		dnsfilter_mac = $('mainTable_table').rows[i].cells[1].title;
		dnsfilter_mode = $('mainTable_table').rows[i].cells[2].childNodes[0].value;

		if(dnsfilter_mode == document.form.dnsfilter_mode.value){
			alert("Cannot specify a rule which matches the global filter mode!");
			$('mainTable_table').rows[i].cells[2].childNodes[0].focus();
			return false;
		}

		dnsfilter_rulelist_tmp += "<";
		dnsfilter_rulelist_tmp += dnsfilter_devname + "&#62";
		dnsfilter_rulelist_tmp += dnsfilter_mac + "&#62";
		dnsfilter_rulelist_tmp += dnsfilter_mode;
	}

	dnsfilter_rule_list = dnsfilter_rulelist_tmp;
	dnsfilter_rule_list_row = dnsfilter_rulelist_tmp.split('<');

	gen_mainTable();
}
</script>
</head>

<body onload="initial();" onunload="unload_body();" onselectstart="return false;">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="DNSFilter.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_dnsfilter">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="dnsfilter_enable_x" value="<% nvram_get("dnsfilter_enable_x"); %>">
<input type="hidden" name="dnsfilter_rulelist" value="">

<table class="content" align="center" cellpadding="0" cellspacing="0" >
	<tr>
		<td width="17">&nbsp;</td>
		<td valign="top" width="202">
		<div  id="mainMenu"></div>
		<div  id="subMenu"></div>
		</td>

    <td valign="top">
	<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0" >
	<tr>
		<td valign="top" >

<table width="730px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
	<tbody>
	<tr>
		<td bgcolor="#4D595D" valign="top">
		<div>&nbsp;</div>
		<div class="formfonttitle">DNS-based Filtering</div>
		<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>

		<div id="dnsfilter_desc" style="margin-bottom:10px;">
			<table width="700px" style="margin-left:25px;">
				<tr>
					<td>&nbsp;&nbsp;</td>
					<td style="font-style: italic;font-size: 14px;">
						<div>
							<p>DNS-based filtering lets you protect specific LAN devices
							against harmful online content by forcing all DNS queries to go through a specified DNS service that blocks malware and/or adult content
							before your browser ever has a chance to load it.</p>
							<br>
							<p>DNS filtering may also be used to force the use of a DNS server required by a specific client.</p>
							<br>
							<p>"No Filtering" will disable/bypass the filter, and "Router" will force clients to use the DNS provided
							by the router's DHCP server (or, the router itself if it's not defined).</p>
							<br>
						</div>
					</td>
				</tr>
			</table>
		</div>
			<!--=====Beginning of Main Content=====-->
			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
				<tr>
					<th>Enable DNS-based Filtering</th>
					<td>
						<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_dnsfilter_enable"></div>
						<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
							<script type="text/javascript">
								$j('#radio_dnsfilter_enable').iphoneSwitch(document.form.dnsfilter_enable_x.value,
									function(){
										document.form.dnsfilter_enable_x.value = 1;
										showhide("dnsfilter_mode",1);
										showhide("dnsfilter_custom1",1);
										showhide("dnsfilter_custom2",1);
										showhide("dnsfilter_custom3",1);
										showhide("mainTable",1);
									},
									function(){
										document.form.dnsfilter_enable_x.value = 0;
										showhide("dnsfilter_mode",0);
										showhide("dnsfilter_custom1",0);
										showhide("dnsfilter_custom2",0);
										showhide("dnsfilter_custom3",0);
										showhide("mainTable",0);
									},
										{
											switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
									});
							</script>
						</div>
					</td>
				</tr>
				<tr id="dnsfilter_mode">
					<th>Global Filter Mode</th>
					<td>
						<select name="dnsfilter_mode" class="input_option">
							<option value="0" <% nvram_match("dnsfilter_mode", "0", "selected"); %>>No filtering</option>
							<option value="11" <% nvram_match("dnsfilter_mode", "11", "selected"); %>>Router</option>
							<option value="1" <% nvram_match("dnsfilter_mode", "1", "selected"); %>>OpenDNS Home</option>
							<option value="7" <% nvram_match("dnsfilter_mode", "7", "selected"); %>>OpenDNS Family</option>
<!--							<option value="2" <% nvram_match("dnsfilter_mode", "2", "selected"); %>>Norton Safe</option> -->
<!--							<option value="3" <% nvram_match("dnsfilter_mode", "3", "selected"); %>>Norton Family</option> -->
<!--							<option value="4" <% nvram_match("dnsfilter_mode", "4", "selected"); %>>Norton Children</option> -->
							<option value="19" <% nvram_match("dnsfilter_mode", "19", "selected"); %>>Adguard Default</option>
							<option value="20" <% nvram_match("dnsfilter_mode", "20", "selected"); %>>Adguard Family</option>
							<option value="5" <% nvram_match("dnsfilter_mode", "5", "selected"); %>>Yandex Safe</option>
							<option value="6" <% nvram_match("dnsfilter_mode", "6", "selected"); %>>Yandex Family</option>
							<option value="12" <% nvram_match("dnsfilter_mode", "12", "selected"); %>>Comodo Secure DNS</option>
							<option value="17" <% nvram_match("dnsfilter_mode", "17", "selected"); %>>Cloudflare Malware</option>
							<option value="18" <% nvram_match("dnsfilter_mode", "18", "selected"); %>>Cloudflare Adult</option>
							<option value="13" <% nvram_match("dnsfilter_mode", "13", "selected"); %>>Quad9</option>
							<option value="14" <% nvram_match("dnsfilter_mode", "14", "selected"); %>>Cleanbrowsing Security</option>
							<option value="15" <% nvram_match("dnsfilter_mode", "15", "selected"); %>>CleanBrowsing Adult</option>
							<option value="16" <% nvram_match("dnsfilter_mode", "16", "selected"); %>>CleanBrowsing Family</option>
							<option value="8" <% nvram_match("dnsfilter_mode", "8", "selected"); %>>Custom 1</option>
							<option value="9" <% nvram_match("dnsfilter_mode", "9", "selected"); %>>Custom 2</option>
							<option value="10" <% nvram_match("dnsfilter_mode", "10", "selected"); %>>Custom 3</option>
						</select>
					</td>
				</tr>
				<tr id="dnsfilter_custom1">
					<th width="200">Custom (user-defined) DNS 1</th>
					<td>
						<input type="text" maxlength="15" class="input_15_table" name="dnsfilter_custom1" value="<% nvram_get("dnsfilter_custom1"); %>" onKeyPress="return is_ipaddr(this,event)">
					</td>
				</tr>
                                <tr id="dnsfilter_custom2">
                                        <th width="200">Custom (user-defined) DNS 2</th>
                                        <td>
                                                <input type="text" maxlength="15" class="input_15_table" name="dnsfilter_custom2" value="<% nvram_get("dnsfilter_custom2"); %>" onKeyPress="return is_ipaddr(this,event)">
                                        </td>
                                </tr>
                                <tr id="dnsfilter_custom3">
                                        <th width="200">Custom (user-defined) DNS 3</th>
                                        <td>
                                                <input type="text" maxlength="15" class="input_15_table" name="dnsfilter_custom3" value="<% nvram_get("dnsfilter_custom3"); %>" onKeyPress="return is_ipaddr(this,event)">
                                        </td>
                                </tr>
			</table>
			<table id="list_table" width="100%" border="0" align="center" cellpadding="0" cellspacing="0" >
				<tr>
					<td valign="top" align="center">
						<!-- client info -->
						<div id="VSList_Block"></div>
						<!-- Content -->
						<div id="mainTable" style="margin-top:10px;"></div>
						<!--br/-->
						<div id="hintBlock" style="width:650px;display:none;"></div>
						<!-- Content -->
					</td>
				</tr>
			</table>
			<div class="apply_gen">
				<input name="button" type="button" class="button_gen" onclick="applyRule()" value="<#CTL_apply#>"/>
			</div>
		</td>
	</tr>
	</tbody>
	</table>
</td>
        </tr>
      </table>
		<!--===================================Ending of Main Content===========================================-->
	</td>

    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</form>
<script>

</script>
</body>
</html>
