<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png"><title><#Web_Title#> - <#Network_Printer_Server#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">

<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

function initial(){
	show_menu();
	$("option5").innerHTML = '<table><tbody><tr><td><div id="index_img5"></div></td><td><div style="width:120px;"><#Menu_usb_application#></div></td></tr></tbody></table>';
	$("option5").className = "m5_r";

	addOnlineHelp($("faq1"), ["ASUSWRT", "ez","printer"]);
	addOnlineHelp($("faq2"), ["ASUSWRT", "lpr"]);
	addOnlineHelp($("faq3"), ["mac", "lpr"]);
	//setTimeout("showMethod('','none');", 100);
}

function showMethod(flag1, flag2){
	document.getElementById("method1").style.display = flag1;
	document.getElementById("method1Title").style.display = flag1;
	document.getElementById("method2").style.display = flag2;
	document.getElementById("method2Title").style.display = flag2;
	if(flag1 == ""){
		$("help1").style.color = "#FFF";
		$("help2").style.color = "gray";
	}
	else{
		$("help1").style.color = "gray";
		$("help2").style.color = "#FFF";
	}
}
</script>
</head>

<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="PrinterServer.asp">
<input type="hidden" name="next_page" value="PrinterServer.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="usblpsrv_enable" value="<% nvram_get("usblpsrv_enable"); %>">

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
<!--=====Beginning of Main Content=====-->
		<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	        <tr>
                <td align="left" valign="top" >

					<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
						<tr>
							<td bgcolor="#4D595D" valign="top"  >
								<div>&nbsp;</div>
								<div style="width:730px">
									<table width="730px">
										<tr>
											<td align="left">
												<span class="formfonttitle"><#Network_Printer_Server#></span>
											</td>
											<td align="right">
												<img onclick="go_setting('/APP_Installation.asp')" align="right" style="cursor:pointer;position:absolute;margin-left:-20px;margin-top:-30px;" title="Back to USB Extension" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
											</td>
										</tr>
									</table>
								</div>
								<div style="margin:5px;"><img src="/images/New_ui/export/line_export.png"></div>
								<div class="formfontdesc"><#Network_Printer_desc#></div>
								<div class="">
									<ul class="">
										<li>
											<a id="faq1" href="" target="_blank" style="text-decoration:underline;font-size:14px;font-weight:bolder;color:#FFF"><#asus_ez_print_share#> FAQ</a>&nbsp;&nbsp;
											<a href="https://dlcdnets.asus.com/pub/ASUS/wireless/RT-AC3200/Printer_1055.zip" style="text-decoration:underline;font-size:14px;font-weight:bolder;color:#FC0"">Download Now!</a>
										</li>
										<li style="margin-top:10px;">
											<a id="faq2" href="" target="_blank" style="text-decoration:underline;font-size:14px;font-weight:bolder;color:#FFF"><#LPR_print_share#> FAQ (Windows)</a>&nbsp;&nbsp;
										</li>
										<li style="margin-top:10px;">
											<a id="faq3" href="" target="_blank" style="text-decoration:underline;font-size:14px;font-weight:bolder;color:#FFF"><#LPR_print_share#> FAQ (MAC)</a>&nbsp;&nbsp;
										</li>
									</ul>
								</div>
								<div>
									<table id="LPRServ" width="98%" border="1" align="center" cellpadding="4" cellspacing="1" bordercolor="#6b8fa3" class="FormTable">
									<!-- <thead>
										<tr><td colspan="2">USB LPR Server</td></tr>
									</thead> -->
										<tr>
										<th>Enable USB LPR Server</th>
										<td>
											<div class="left" style="width:94px; position:relative; left:3%;" id="radio_usblpsrv_enable"></div>
												<div class="clear"></div>
												<script type="text/javascript">
												$j('#radio_usblpsrv_enable').iphoneSwitch('<% nvram_get("usblpsrv_enable"); %>',
													 function() {
														document.form.usblpsrv_enable.value = 1;
														document.form.action_script.value = "restart_usblpsrv";
														parent.showLoading();
														document.form.submit();
														return true;
													 },
													 function() {
														document.form.usblpsrv_enable.value = 0;
														document.form.action_script.value = "stop_usblpsrv";
														parent.showLoading();
														document.form.submit();
														return true;
													 },
													 {
														switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
													 }
												);
												</script>
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

<!--=====End of Main Content=====-->
	</td>
	  <td width="10" align="center" valign="top">&nbsp;</td>
  </tr>
</table>
</form>

<div id="footer"></div>
</body>
</html>
