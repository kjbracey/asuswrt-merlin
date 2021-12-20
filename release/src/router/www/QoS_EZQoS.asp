<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#EZQoS#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/general.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();
</script>
<!--style type="text/css">
.qos_table{
	width:740px;
	padding:10px; 
	margin-top:-17px;
	position:relative;
	background-color:#4d595d;
	height: 650px;
	-webkit-border-bottom-right-radius: 3px;
	-webkit-border-bottom-left-radius: 3px;
	-moz-border-radius-bottomright: 3px;
	-moz-border-radius-bottomleft: 3px;
	border-bottom-right-radius: 3px;
	border-bottom-left-radius: 3px;
}
</style-->
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
var productid = '<% nvram_get("productid"); %>';
var qos_type = '<% nvram_get("qos_type"); %>';
var qos_rulelist_array = "<% nvram_char_to_ascii("","qos_rulelist"); %>";
var qos_bw_rulelist_array = "<% nvram_get("qos_bw_rulelist"); %>".replace(/&#62/g, ">").replace(/&#60/g, "<");
var ctf_disable = '<% nvram_get("ctf_disable"); %>';
var ctf_disable_force = '<% nvram_get("ctf_disable_force"); %>';
var ctf_fa_mode = '<% nvram_get("ctf_fa_mode"); %>';
var value1K = 1000;

if ((based_modelid == "RT-AC56U") || (based_modelid == "RT-AC68U")) {
	var codel_support = true;
} else {
	var codel_support = false;
}

var overlib_str0 = new Array();	//Viz add 2011.06 for record longer qos rule desc
var overlib_str = new Array();	//Viz add 2011.06 for record longer portrange value

function initial(){
	show_menu();

	if(downsize_4m_support)
		$("guest_image").parentNode.style.display = "none";

	if(qos_type == 0){
		document.form.qos_obw.parentNode.parentNode.style.display = "";
		document.form.qos_ibw.parentNode.parentNode.style.display = "";
		document.form.qos_default.parentNode.parentNode.style.display = "";
		if (codel_support)
			document.getElementById('qos_sched_tr').style.display = "";
		document.getElementById('qos_overhead_tr').style.display = "";
	}else{
		document.form.qos_obw.parentNode.parentNode.style.display = "none";
		document.form.qos_ibw.parentNode.parentNode.style.display = "none";
		document.form.qos_default.parentNode.parentNode.style.display = "none";
		if (codel_support)
			document.getElementById('qos_sched_tr').style.display = "";
		document.getElementById('qos_overhead_tr').style.display = "none";
	}

	if(document.form.qos_enable.value == "1")
		document.getElementById('qos_suspend').style.display = "";
	else
		document.getElementById('qos_suspend').style.display = "none";

	init_changeScale("qos_obw");
	init_changeScale("qos_ibw");
	if(qos_type == "0"){
		showqos_rulelist();
	}
	else if(qos_type == "2"){
		showqos_bw_rulelist();
	}

	addOnlineHelp($("faq"), ["ASUSWRT", "QoS"]);
}

function changeRule(obj){
	if($(obj).value == "0"){
		document.form.qos_obw.parentNode.parentNode.style.display = "";
		document.form.qos_ibw.parentNode.parentNode.style.display = "";
		document.form.qos_default.parentNode.parentNode.style.display = "";
		if (codel_support)
			document.getElementById('qos_sched_tr').style.display = "";
		else
			document.getElementById('qos_sched_tr').style.display = "none";
		document.getElementById('qos_overhead_tr').style.display = "";
		showqos_rulelist();
	}else if($(obj).value == "2"){
		document.form.qos_obw.parentNode.parentNode.style.display = "none";
		document.form.qos_ibw.parentNode.parentNode.style.display = "none";
		document.form.qos_default.parentNode.parentNode.style.display = "none";
		if (codel_support)
			document.getElementById('qos_sched_tr').style.display = "";
		else
			document.getElementById('qos_sched_tr').style.display = "none";
		document.getElementById('qos_overhead_tr').style.display = "none";
		showqos_bw_rulelist();
	}

	if($(obj).value == "0"){
		showqos_rulelist();
	}
	else if($(obj).value == "2"){
		showqos_bw_rulelist();
	}
}

function init_changeScale(_obj_String){
	if($(_obj_String).value > 999){
		$(_obj_String+"_scale").value = "Mb/s";
		$(_obj_String).value = Math.round(($(_obj_String).value/value1K)*1000)/1000;
	}
}

function changeScale(_obj_String){
	if($(_obj_String+"_scale").value == "Mb/s")
		$(_obj_String).value = Math.round(($(_obj_String).value/value1K)*1000)/1000;
	else
		$(_obj_String).value = Math.round($(_obj_String).value*value1K);
}

function switchPage(page){
	if(page == "2")	
		location.href = "/Advanced_QOSUserRules_Content.asp";
	else if(page == "3")
		location.href = "/Advanced_QOSUserPrio_Content.asp";
	else if(page == "4")
		location.href = "/Bandwidth_Limiter.asp";
	else
		return false;
}

function applyRule(){
	if(document.form.qos_type[1].checked)
		document.form.qos_type.value = "2";
	else
		document.form.qos_type.value = "0";

	if(document.form.qos_type.value == 0){
		if(document.form.qos_obw.value.length == 0 || document.form.qos_obw.value == 0){
			alert("<#JS_fieldblank#>");
			document.form.qos_obw.focus();
			document.form.qos_obw.select();
			return false;
		}
		if(document.form.qos_ibw.value.length == 0 || document.form.qos_ibw.value == 0){
			alert("<#JS_fieldblank#>");
			document.form.qos_ibw.focus();
			document.form.qos_ibw.select();
			return false;
		}
	}

	if($("qos_obw_scale").value == "Mb/s")
		document.form.qos_obw.value = Math.round(document.form.qos_obw.value*value1K);
	if($("qos_ibw_scale").value == "Mb/s")
		document.form.qos_ibw.value = Math.round(document.form.qos_ibw.value*value1K);

	if(document.form.qos_enable.value == "0")
		document.form.action_script.value = "";
	else {
		if(ctf_disable == 1){
			if(document.form.qos_suspend.value != document.form.qos_suspend_orig.value){
				if(document.form.qos_suspend.value == "1")
					document.form.action_script.value = "stop_qos";
				else
					document.form.action_script.value = "start_qos";
			}
			else{
				if(document.form.qos_suspend.value == "0")
					document.form.action_script.value = "restart_qos";
				else
					document.form.action_script.value = "";
			}
		}
		else{
			if(ctf_fa_mode == "2"){
				FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
			}
			else{
				document.form.action_script.value = "restart_qos";
			}
		}
	}

	parent.showLoading();
	document.form.submit();
}

function submitQoS(){
	if(document.form.qos_type[1].checked)
		document.form.qos_type.value = "2";
	else
		document.form.qos_type.value = "0";

	// Jieming To Do: please add a hint here when error occurred, and qos_ibw & qos_obw should allow number only.
	if(document.form.qos_type.value == 0){
		if(document.form.qos_obw.value.length == 0 || document.form.qos_obw.value == 0){
				document.form.qos_enable.value = document.form.qos_enable_orig.value;
				$j('#radio_qos_enable').iphoneSwitch(document.form.qos_enable.value);
				alert("<#JS_fieldblank#>");
				document.form.qos_obw.focus();
				document.form.qos_obw.select();
				return;
		}
		if(document.form.qos_ibw.value.length == 0 || document.form.qos_ibw.value == 0){
				document.form.qos_enable.value = document.form.qos_enable_orig.value;
				$j('#radio_qos_enable').iphoneSwitch(document.form.qos_enable.value);
				alert("<#JS_fieldblank#>");
				document.form.qos_ibw.focus();
				document.form.qos_ibw.select();
				return;
		}
	// end

		if($("qos_obw_scale").value == "Mb/s")
			document.form.qos_obw.value = Math.round(document.form.qos_obw.value*value1K);
		if($("qos_ibw_scale").value == "Mb/s")
			document.form.qos_ibw.value = Math.round(document.form.qos_ibw.value*value1K);
	}
	else {
		if(qos_bw_rulelist_array.length == 0){
			document.form.qos_enable.value = document.form.qos_enable_orig.value;
			$j('#radio_qos_enable').iphoneSwitch(document.form.qos_enable.value);
			alert("Please define Bandwidth Limiter rules before enabling the Bandwidth Limiter!");
			return;
		}
	}

	if(document.form.qos_enable.value != document.form.qos_enable_orig.value){
		document.form.qos_suspend.value = "0";
		if(ctf_disable_force == "0")
			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
		else {
			if(document.form.qos_enable.value == "0")
				document.form.action_script.value = "stop_qos";
			else
				document.form.action_script.value = "start_qos";
		}
	}

	parent.showLoading();
	document.form.submit();	
	
}

function showqos_rulelist(){
	var qos_rulelist_row = "";
	qos_rulelist_row = decodeURIComponent(qos_rulelist_array).split('<');
	var client_list_array = '<% get_client_detail_info(); %>';
	var client_list_row = client_list_array.split('<');

	var code = "";
// table header
	code +='<table style="margin-left:20px;" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">';
	code +='<thead>';
	code +='<tr>';
	code +="<td colspan=\"6\" id=\"TriggerList\" style=\"border-right:none;height:22px;\"><#BM_UserList_title#></td>";
	code +='</tr>';
	code +='</thead>';
	code +='<tr>';
	code +='<th width="22%" style="height:30px;"><#BM_UserList1#></th>';
	code +='<th width="21%"><a href="javascript:void(0);" onClick="openHint(18,6);"><div class="table_text">Source IP or MAC</div></a></th>';
	code +='<th width="17%"><a href="javascript:void(0);" onClick="openHint(18,4);"><div class="table_text"><#BM_UserList3#></div></a></th>';
	code +='<th width="14%"><div class="table_text"><#IPConnection_VServerProto_itemname#></div></th>';
	code +='<th width="16%"><a href="javascript:void(0);" onClick="openHint(18,5);"><div class="table_text"><div class="table_text">Transferred</div></a></th>';
	code +='<th width="12%"><#BM_UserList4#></th>';
	code +='</tr>';
	code +='</table>';
//table data
	code +='<table style="margin-left:20px;margin-bottom:30px;" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="list_table" id="qos_rulelist_table">';
	if(qos_rulelist_row.length == 1)	// no exist "<"
		code +='<tr><td style="color:#FFCC00;height:30px;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i = 1; i < qos_rulelist_row.length; i++){
			overlib_str0[i] ="";
			overlib_str[i] ="";			
			code +='<tr id="row'+i+'">';
			var qos_rulelist_col = qos_rulelist_row[i].split('>');
			var wid=[22, 21, 17, 14, 16, 12];						
				for(var j = 0; j < qos_rulelist_col.length; j++){
						if(j != 0 && j != 1 && j !=2 && j!=5){
							code +='<td width="'+wid[j]+'%" style="height:30px;">'+ qos_rulelist_col[j] +'</td>';
						}else if(j==0){
							if(qos_rulelist_col[0].length >15){
								overlib_str0[i] += qos_rulelist_col[0];
								qos_rulelist_col[0]=qos_rulelist_col[0].substring(0, 13)+"...";
								code +='<td width="'+wid[j]+'%"  title="'+overlib_str0[i]+'" style="height:30px;">'+ qos_rulelist_col[0] +'</td>';
							}else
								code +='<td width="'+wid[j]+'%" style="height:30px;">'+ qos_rulelist_col[j] +'</td>';
						}else if(j==1){
							var apps_client_name = "";
							var apps_client_id = qos_rulelist_col[1];
							for(var k = 1; k < client_list_row.length; k += 1) {
								var client_list_col = client_list_row[k].split('>');
								if(apps_client_id == client_list_col[3]){ // lookup name based on mac
									apps_client_name = client_list_col[1];
								}
								if(apps_client_id == client_list_col[2]){ // lookup name based on ipaddr
									apps_client_name = client_list_col[1];
								}
								if(apps_client_name != "")
									break;
							}
							if(apps_client_name != "")
								code += '<td width="'+wid[1]+'%" title="' + apps_client_id + '">'+ apps_client_name + '<br>(' +  apps_client_id +')</td>';
							else
								code += '<td width="'+wid[1]+'%" title="' + apps_client_id + '">' + apps_client_id + '</td>';
						}else if(j==2){
							if(qos_rulelist_col[2].length >13){
								overlib_str[i] += qos_rulelist_col[2];
								qos_rulelist_col[2]=qos_rulelist_col[2].substring(0, 11)+"...";
								code +='<td width="'+wid[j]+'%"  title="'+overlib_str[i]+'" style="height:30px;">'+ qos_rulelist_col[2] +'</td>';
							}else
								code +='<td width="'+wid[j]+'%" style="height:30px;">'+ qos_rulelist_col[j] +'</td>';
						}else if(j==5){
								code += '<td width="'+wid[j]+'%" style="height:30px;">';

								if(qos_rulelist_col[5] =="0")
									code += '<#Highest#>';
								if(qos_rulelist_col[5] =="1")
									code += '<#High#>';
								if(qos_rulelist_col[5] =="2")
									code += '<#Medium#>';
								if(qos_rulelist_col[5] =="3")
									code += '<#Low#>';
								if(qos_rulelist_col[5] =="4")
									code += '<#Lowest#>';
						}
						code +='</td>';
				}
				code +='</tr>';
		}
	}
	code +='</table>';
	$("qos_current_rulelist").innerHTML = code;
	
	parse_port="";
}

function showqos_bw_rulelist(){
	var qos_bw_rulelist_row = "";
	qos_bw_rulelist_row = decodeURIComponent(qos_bw_rulelist_array).split('<');

	var code = "";
// table header
	code +='<table style="margin-left:20px;" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table">';
	code +='<thead>';
	code +='<tr>';
	code +="<td colspan=\"6\" id=\"TriggerList\" style=\"border-right:none;height:22px;\"><#BM_UserList_title#></td>";
	code +='</tr>';
	code +='</thead>';
	code +='<tr>';
	code +='<th width="15%"><div class="table_text">Enabled</th>';
	code +='<th width="35%"><a href="javascript:void(0);" onClick="openHint(18,6);"><div class="table_text"><#NetworkTools_target#></div></a></th>';
	code +='<th width="25%"><div class="table_text"><#download_bandwidth#></th>';
	code +='<th width="25%"><div class="table_text"><#upload_bandwidth#></div></th>';
	code +='</tr>';
	code +='</table>';
//table data
	code +='<table style="margin-left:20px;margin-bottom:30px;" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="list_table" id="qos_bw_rulelist_table">';
	if(qos_bw_rulelist_row.length == 1)	// no exist "<"
		code +='<tr><td style="color:#FFCC00;height:30px;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		var client_list_array = '<% get_client_detail_info(); %>';
		var client_list_row = client_list_array.split('<');
		for(var i = 1; i < qos_bw_rulelist_row.length; i++){
			overlib_str0[i] ="";
			overlib_str[i] ="";
			code +='<tr id="row'+i+'">';
			var qos_bw_rulelist_col = qos_bw_rulelist_row[i].split('>');
			var wid=[15, 35, 25, 25];
				for(var j = 0; j < qos_bw_rulelist_col.length; j++){
						if(j==0){
							if(qos_bw_rulelist_col[0] == "1")
								code +='<td width="'+wid[j]+'%" style="height:30px;">'+ '<#checkbox_Yes#>' +'</td>';
							else
								code +='<td width="'+wid[j]+'%" style="height:30px;">'+ '<#checkbox_No#>' +'</td>';
						}else if(j==1){
							var apps_client_name = "";
							var apps_client_id = qos_bw_rulelist_col[1];
							for(var k = 1; k < client_list_row.length; k += 1) {
								var client_list_col = client_list_row[k].split('>');
								if(apps_client_id == client_list_col[3]){ // lookup name based on mac
									apps_client_name = client_list_col[1];
								}
								if(apps_client_id == client_list_col[2]){ // lookup name based on ipaddr
									apps_client_name = client_list_col[1];
								}
								if(apps_client_name != "")
								break;
							}
							if(apps_client_name != "")
								code += '<td width="'+wid[j]+'%" style="height:30px;">'+ apps_client_name +'<br>(' +  apps_client_id +')</td>';
							else
								code += '<td width="'+wid[j]+'%" style="height:30px;">'+ apps_client_id +'</td>';
						}else if(j==2){
							code += '<td width="'+wid[j]+'%" style="text-align:center;">'+Math.round((qos_bw_rulelist_col[2]/value1K)*1000)/1000+' Mb/s</td>';
						}else if(j==3){
							code += '<td width="'+wid[j]+'%" style="text-align:center;">'+Math.round((qos_bw_rulelist_col[3]/value1K)*1000)/1000+' Mb/s</td>';
						}
				}
				code +='</tr>';
		}
	}
	code +='</table>';
	$("qos_current_rulelist").innerHTML = code;
	
	parse_port="";
}

</script>
</head>

<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="/QoS_EZQoS.asp">
<input type="hidden" name="next_page" value="/QoS_EZQoS.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_qos">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="flag" value="">
<input type="hidden" name="qos_enable" value="<% nvram_get("qos_enable"); %>">
<input type="hidden" name="qos_enable_orig" value="<% nvram_get("qos_enable"); %>">
<input type="hidden" name="qos_suspend_orig" value="<% nvram_get("qos_suspend"); %>">
<input type="hidden" name="qos_type_orig" value="<% nvram_get("qos_type"); %>">
<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>
	
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>
	
    <td valign="top">
			<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
		<table width="95%" border="0" align="left" cellpadding="0" cellspacing="0" class="FormTitle" id="FormTitle">
  		<tr>
    			<td bgcolor="#4D595D" valign="top">
    				<table width="760px" border="0" cellpadding="4" cellspacing="0">
        			<tr>
						<td bgcolor="#4D595D" valign="top">
							<table width="100%">
								<tr>
									<td  class="formfonttitle" align="left">								
										<div ><#Menu_TrafficManager#> - QoS</div>
									</td>
									<td align="right" >
										<div>
											<select onchange="switchPage(this.options[this.selectedIndex].value)" class="input_option">
												<!--option><#switchpage#></option-->
												<option value="1" selected><#qos_automatic_mode#></option>
												<option value="2"><#qos_user_rules#></option>
												<option value="3"><#qos_user_prio#></option>
												<option value="4">User-defined Bandwidth Limiting</option>
											</select>	    
										</div>
									</td>	
								</tr>
							</table>	
						</td>
        			</tr>
        			<tr>
          				<td height="5" bgcolor="#4D595D" valign="top"><img src="images/New_ui/export/line_export.png" /></td>
        			</tr>
        			<tr>
          				<td height="30" align="left" valign="top" bgcolor="#4D595D">
										<div>
											<table width="650px">
												<tr>
													<td width="130px">
														<img id="guest_image" src="/images/New_ui/QoS.png">
													</td>
													<td style="font-style: italic;font-size: 14px;">
														<div class="formfontdesc" style="line-height:20px;"><#ezqosDesw#><br>Bandwidth Limiter allows you to control the max connection speed of the client device. You can select the host name from target or fill in IP address / IP Range / MAC address for limited speed profile setting.</div>
														<div class="formfontdesc">
															<a id="faq" href="" target="_blank" style="text-decoration:underline;">QoS FAQ</a>
														</div>
													</td>
												</tr>
											</table>
										</div>
          				</td>
        			</tr>
							
							<tr>
								<td valign="top">
									<table style="margin-left:3px;" width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<tr>
										<th><#Enable_defaule_rule#></th>
											<td>
												<div class="left" style="width:94px; float:left; cursor:pointer;" id="radio_qos_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:160px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_qos_enable').iphoneSwitch('<% nvram_get("qos_enable"); %>', 
														 function() {
															document.form.qos_enable.value = "1";
															submitQoS();

														 },
														 function() {
															document.form.qos_enable.value = "0";
															submitQoS();

														 },
														 {
															switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														 }
													);
												</script>
												<select id="qos_suspend" name="qos_suspend" class="input_option" style="display:none; margin-top:4px;">
													<option value="0" <% nvram_match("qos_suspend", "0","selected"); %>>Active</option>
													<option value="1" <% nvram_match("qos_suspend", "1","selected"); %>>Suspended</option>
												</select>
												</div>	
											</td>
										</tr>
										<tr id="qos_type_tr">
											<th>QoS Type</th>
											<td colspan="2">
												<input id="trad_type" name="qos_type" value="0" type="radio" <% nvram_match("qos_type", "0","checked"); %> onClick="changeRule(this);">Traditional QoS
												<input id="bw_limit_type" name="qos_type" value="2" type="radio" <% nvram_match("qos_type", "2","checked"); %> onClick="changeRule(this);">Bandwidth Limiter
											</td>
										</tr>

										<tr id="qos_sched_tr" style="display:none">
											<th>Queueing Discipline</th>
											<td>
												<select name="qos_sched" class="input_option">
													<option value="0" <% nvram_match("qos_sched", "0","selected"); %>>SFQ</option>
													<option value="1" <% nvram_match("qos_sched", "1","selected"); %>>CODEL</option>
													<option value="2" <% nvram_match("qos_sched", "2","selected"); %>>FQ_CODEL (Default)</option>
												</select>
											</td>
										</tr>

										<tr id="qos_overhead_tr" style="display:none">
											<th>WAN packet overhead</th>
											<td colspan="2">
												<select name="qos_overhead" class="input_option" >
													<option value="0" <% nvram_match("qos_overhead", "0","selected"); %>>0-None (Not a DSL connection)</option>
													<option value="148" <% nvram_match("qos_overhead", "148","selected"); %>>48(ATM)-Conservative default</option>
													<option value="84038" <% nvram_match("qos_overhead", "84038","selected"); %>>38(min 84)-Ethernet</option>
													<option value="84042" <% nvram_match("qos_overhead", "84042","selected"); %>>42(min 84)-Ethernet with VLAN</option>
													<option value="64018" <% nvram_match("qos_overhead", "64018","selected"); %>>18(min 64)-Cable (DOCSIS)</option>
													<option value="132" <% nvram_match("qos_overhead", "132","selected"); %>>32(ATM)-ADSL PPPoE VC-Mux (Router authentication)</option>
													<option value="140" <% nvram_match("qos_overhead", "140","selected"); %>>40(ATM)-ADSL PPPoE LLC/Snap (Router authentication)</option>
													<option value="108" <% nvram_match("qos_overhead", "108","selected"); %>>8(ATM)-ADSL IPoA RFC2684/RFC1483 Routed VC-Mux (Modem authentication)</option>
													<option value="116" <% nvram_match("qos_overhead", "116","selected"); %>>16(ATM)-ADSL IPoA RFC2684/RFC1483 Routed LLC/Snap (Modem authentication)</option>
													<option value="124" <% nvram_match("qos_overhead", "124","selected"); %>>24(ATM)-ADSL DHCP RFC2684/RFC1483 Bridged VC-Mux</option>
													<option value="100132" <% nvram_match("qos_overhead", "100132","selected"); %>>32(ATM)-ADSL DHCP RFC2684/RFC1483 Bridged LLC/Snap</option>
													<option value="110" <% nvram_match("qos_overhead", "110","selected"); %>>10(ATM)-ADSL PPPoA RFC2684/RFC1483 Routed VC-Mux</option>
													<option value="114" <% nvram_match("qos_overhead", "114","selected"); %>>14(ATM)-ADSL PPPoA RFC2684/RFC1483 Routed LLC</option>
													<option value="230" <% nvram_match("qos_overhead", "230","selected"); %>>30(PTM)-VDSL2 PPPoE PTM</option>
													<option value="222" <% nvram_match("qos_overhead", "222","selected"); %>>22(PTM)-VDSL2 Bridged PTM</option>
												</select>
											</td>
										</tr>

										<tr>
											<th><#upload_bandwidth#></th>
											<td>
													<input type="text" maxlength="10" id="qos_obw" name="qos_obw" onKeyPress="return is_decimal(this,event);" class="input_15_table" value="<% nvram_get("qos_obw"); %>">
														<select id="qos_obw_scale" class="input_option" style="width:87px;" onChange="changeScale('qos_obw');">
															<option value="Kb/s">Kb/s</option>
															<option value="Mb/s">Mb/s</option>
														</select>
											</td>
										</tr>
										
										<tr>
											<th><#download_bandwidth#></th>
											<td>
													<input type="text" maxlength="10" id="qos_ibw" name="qos_ibw" onKeyPress="return is_decimal(this,event);" class="input_15_table" value="<% nvram_get("qos_ibw"); %>">
														<select id="qos_ibw_scale" class="input_option" style="width:87px;" onChange="changeScale('qos_ibw');">
															<option value="Kb/s">Kb/s</option>
															<option value="Mb/s">Mb/s</option>
														</select>
											</td>
										</tr>

										<tr id="qos_default">
											<th>Default Priority Level</th>
											<td>
												<select name="qos_default" class="input_option">
													<option value="0" <% nvram_match("qos_default", "0","selected"); %>>Highest</option>
													<option value="1" <% nvram_match("qos_default", "1","selected"); %>>High</option>
													<option value="2" <% nvram_match("qos_default", "2","selected"); %>>Medium</option>
													<option value="3" <% nvram_match("qos_default", "3","selected"); %>>Low (Default)</option>
													<option value="4" <% nvram_match("qos_default", "4","selected"); %>>Lowest</option>
												</select>
												<!-- <span>&nbsp;&nbsp;(Default level is also used by any VPN connections)</span> -->
											</td>
										</tr>

									</table>
								</td>
				</tr>	

        			<tr>
          				<td height="50" >
						<div style=" *width:136px;margin-left:300px;" id="applybutton" style="color:#FFFFFF" class="titlebtn" align="center" onClick="applyRule();"><span><#CTL_onlysave#></span></div>
          				</td>
        			</tr>
			</table>
			<table id="list_table" width="94%" border="0" cellpadding="0" cellspacing="0" style="padding-left:8px;">
				<tr>
					<td valign="top" align="center">
						<div id="mainTable" style="margin-top:10px;"></div>
						<div id="qos_current_rulelist"></div>
					</td>
				</tr>
			</table>
      		</td>  
      	</tr>
		</table>
		<!--===================================End of Main Content===========================================-->
		</td>
		
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
