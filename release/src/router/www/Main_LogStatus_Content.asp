﻿<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="X-UA-Compatible" content="IE=edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu5_7_2#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/jquery.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

var $j = jQuery.noConflict();

function showclock(){
	JS_timeObj.setTime(systime_millsec);
	systime_millsec += 1000;
	JS_timeObj2 = JS_timeObj.toString();	
	JS_timeObj2 = JS_timeObj2.substring(0,3) + ", " +
	              JS_timeObj2.substring(4,10) + "  " +
				  checkTime(JS_timeObj.getHours()) + ":" +
				  checkTime(JS_timeObj.getMinutes()) + ":" +
				  checkTime(JS_timeObj.getSeconds()) + "  " +
				  /*JS_timeObj.getFullYear() + " GMT" +
				  timezone;*/ // Viz remove GMT timezone 2011.08
				  JS_timeObj.getFullYear();
	$("system_time").value = JS_timeObj2;
	setTimeout("showclock()", 1000);
	if(navigator.appName.indexOf("Microsoft") >= 0)
		document.getElementById("textarea").style.width = "99%";
    //$("banner3").style.height = "13";
}

function update_upTime(){
	$j.ajax({
		url: '/ajax_uptime.asp',
		dataType: 'script',
		error: function(xhr){
			setTimeout("update_upTime();", 1000);
		},
		success: function(response){
			timenow = systime_epoch;
			systime_epoch += 1;
			uptime = parseInt(uptimeStr.substring(32,42));

			//System
			Days = Math.floor(uptime / (60*60*24));	
			Hours = Math.floor((uptime / 3600) % 24);
			Minutes = Math.floor(uptime % 3600 / 60);
			Seconds = Math.floor(uptime % 60);
	
			$("boot_days").innerHTML = Days;
			$("boot_hours").innerHTML = Hours;
			$("boot_minutes").innerHTML = Minutes;
			$("boot_seconds").innerHTML = Seconds;

			if (sw_mode != 1) {
				document.getElementById('wan_total').style.display = "none";
				document.getElementById('wan_current').style.display = "none";
			} else {
				if (wanstart > 0) {
					wantime = timenow - parseInt(wanstart);  //calc wan time
					wantotal = parseInt(wanuptime) + wantime - parseInt(wanbootdelay);
					document.getElementById('wan_total').style.display = "";
					if (wantime != wantotal) {
						document.getElementById('wan_current').style.display = "";
						document.getElementById('wantotal_th').innerHTML = "WAN " + "<#General_x_SystemUpTime_itemname#>" + " (Total)";
					}
					else {
						document.getElementById('wan_current').style.display = "none";
						document.getElementById('wantotal_th').innerHTML = "WAN " + "<#General_x_SystemUpTime_itemname#>" + " (Total / Current)	";
					}
					document.getElementById('wan_status').style.display = "none";
				}
				else if (wanstart == 0) {
					wantime = uptime - parseInt(wanbootdelay);  //wan up during boot, time not yet set
					wantotal = wantime;
					document.getElementById('wan_total').style.display = "";
					document.getElementById('wan_current').style.display = "none";
					document.getElementById('wantotal_th').innerHTML = "WAN " + "<#General_x_SystemUpTime_itemname#>" + " (Total / Current)";
					document.getElementById('wan_status').style.display = "none";
				}
				else {
					wantime = 0; //wan is down
					wantotal = parseInt(wanuptime) - parseInt(wanbootdelay);
					document.getElementById('wan_total').style.display = "";
					document.getElementById('wan_current').style.display = "";
					document.getElementById('wantotal_th').innerHTML = "WAN " + "<#General_x_SystemUpTime_itemname#>" + " (Total)";
					document.getElementById('wan_status').style.display = "";
				}

				//Current
				Days = Math.floor(wantime / (60*60*24));
				Hours = Math.floor((wantime / 3600) % 24);
				Minutes = Math.floor(wantime % 3600 / 60);
				Seconds = Math.floor(wantime % 60);

				if (Days < 0 || Hours < 0 || Minutes < 0 || Seconds < 0) {  //block transients during up/down
					Days = 0; Hours = 0; Minutes = 0; Seconds = 0;
				}

				$("wan_days").innerHTML = Days;
				$("wan_hours").innerHTML = Hours;
				$("wan_minutes").innerHTML = Minutes;
				$("wan_seconds").innerHTML = Seconds;

				//Total
				Days = Math.floor(wantotal / (60*60*24));
				Hours = Math.floor((wantotal / 3600) % 24);
				Minutes = Math.floor(wantotal % 3600 / 60);
				Seconds = Math.floor(wantotal % 60);

				if (Days >=0 && Hours >=0 && Minutes >=0 && Seconds >= 0) {  //block transients during up/down
					$("wan_tdays").innerHTML = Days;
					$("wan_thours").innerHTML = Hours;
					$("wan_tminutes").innerHTML = Minutes;
					$("wan_tseconds").innerHTML = Seconds;
				}
			}

			setTimeout("update_upTime();", 1000);
		}
	});
}

function clearLog(){
	document.form1.target = "hidden_frame";
	document.form1.action_mode.value = " Clear ";
	document.form1.submit();
	location.href = location.href;
}

function showDST(){
	var system_timezone_dut = "<% nvram_get("time_zone"); %>";
	if((system_timezone_dut.search("DST") >= 0 || system_timezone_dut.search("TDT")) >= 0 && "<% nvram_get("time_zone_dst"); %>" == "1"){
		document.getElementById('dstzone').style.display = "";
		document.getElementById('dstzone').innerHTML = "<#General_x_SystemTime_dst#>";
	}
}

function initial(){
	show_menu();
	showclock();
	showDST();
	update_upTime();
	var retArea = document.getElementById('textarea');
	retArea.scrollTop = retArea.scrollHeight - retArea.clientHeight; //make Scroll_y bottom - IE fix 
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" action="apply.cgi" target="hidden_frame">
<input type="hidden" name="current_page" value="Main_LogStatus_Content.asp">
<input type="hidden" name="next_page" value="Main_LogStatus_Content.asp">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
</form>
<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>
		<td valign="top" width="202">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>	
		<td valign="top">
			<div id="tabMenu" class="submenuBlock"></div>		
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td align="left" valign="top">				
						<table width="760px" border="0" cellpadding="5" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle">		
							<tr>
								<td bgcolor="#4D595D" colspan="3" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle"><#System_Log#> - <#menu5_7_2#></div>
									<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
									<div class="formfontdesc"><#GeneralLog_title#></div>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable">
										<tr>
											<th width="20%"><#General_x_SystemTime_itemname#></th>
											<td>
												<input type="text" id="system_time" name="system_time" size="40" class="devicepin" value="" readonly="1" style="font-size:12px;">
												<br><span id="dstzone" style="display:none;margin-left:5px;color:#FFFFFF;"></span>
											</td>										
										</tr>
										<tr>
											<th id="uptime_th"><!--a class="hintstyle" href="javascript:void(0);" onClick="openHint(12, 1);"--><#General_x_SystemUpTime_itemname#></a></th>
											<td><span id="boot_days"></span> <#Day#> <span id="boot_hours"></span> <#Hour#> <span id="boot_minutes"></span> <#Minute#> <span id="boot_seconds"></span> <#Second#></td>
										</tr>
										<tr id="wan_total" style="display:none">
											<th id="wantotal_th">WAN <#General_x_SystemUpTime_itemname#> (Total)</th>
											<td><span id="wan_tdays"></span> <#Day#> <span id="wan_thours"></span> <#Hour#> <span id="wan_tminutes"></span> <#Minute#> <span id="wan_tseconds"></span> <#Second#></td>
										</tr>
										<tr id="wan_current" style="display:none;">
											<th>WAN <#General_x_SystemUpTime_itemname#> (Current)</th>
											<td><span id="wan_days"></span> <#Day#> <span id="wan_hours"></span> <#Hour#> <span id="wan_minutes"></span> <#Minute#> <span id="wan_seconds"></span> <#Second#><span id="wan_status" style="display:none;margin-left:20px;">WAN is down</span></td>
										</tr>
									</table>
									<div style="margin-top:8px">
										<textarea cols="63" rows="52" wrap="off" readonly="readonly" id="textarea" style="width:99%; font-family:'Courier New', Courier, mono; font-size:11px;background:#475A5F;color:#FFFFFF;"><% nvram_dump("syslog.log","syslog.sh"); %></textarea>
									</div>
									<div>
									<table class="apply_gen">
										<tr class="apply_gen" valign="top">
											<td width="40%" align="right">
												<form method="post" name="form1" action="apply.cgi">
													<input type="hidden" name="current_page" value="Main_LogStatus_Content.asp">
													<input type="hidden" name="action_mode" value=" Clear ">
													<input type="submit" onClick="onSubmitCtrl(this, ' Clear ')" value="<#CTL_clear#>" class="button_gen">
												</form>
											</td>	
											<td width="20%" align="center">
												<form method="post" name="form2" action="syslog.txt">
													<input type="submit" onClick="onSubmitCtrl(this, ' Save ');" value="<#CTL_onlysave#>" class="button_gen">
												</form>
											</td>	
											<td width="40%" align="left" >
											<form method="post" name="form3" action="apply.cgi">
												<input type="hidden" name="current_page" value="Main_LogStatus_Content.asp">
												<input type="hidden" name="action_mode" value=" Refresh ">
												<input type="button" onClick="location.href=location.href" value="<#CTL_refresh#>" class="button_gen">
											</form>
											</td>	
										</tr>
									</table>
									</div>
								</td>
							</tr>
						</table>
					</td>
				</tr>
			</table>
			<!--===================================Ending of Main Content===========================================-->
		</td>
		<td width="10" align="center" valign="top"></td>
	</tr>
</table>
<div id="footer"></div>
</body>
</html>
