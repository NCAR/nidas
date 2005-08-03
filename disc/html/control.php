<link href='index.css' rel='stylesheet' type='text/css'>

<!-- ----------------------------------------------------------------------- -->
<!-- Query the dsm_server for a list of DSM names and locations.             -->
<!-- ----------------------------------------------------------------------- -->
<?php
include_once('utils/utils.php');

$dsmList = xu_rpc_http_concise( array( 'method' => 'GetDsmList',
                                       'args' => '',
                                       'host' => 'localhost',
      'uri' => '/RPC2', 'port' => '50002', 'debug' => '0', 'output' => 'xmlrpc'));

// TODO - see if xu_rpc_http_concise provides any status variables to test here instead.
if ($dsmList == "")
  exit("<h5>DSM server not responding!</h5>");

if (empty($dsmList))
  exit('<h5>Cannot receive list of DSMs from the server!</h5>');
?>

<!-- ----------------------------------------------------------------------- -->
<!-- Query the dsm_server for a list of project directories.                 -->
<!-- ----------------------------------------------------------------------- -->
<?php
$projList = xu_rpc_http_concise( array( 'method' => 'GetProjectList',
                                        'args' => '',
                                        'host' => 'localhost',
      'uri' => '/RPC2', 'port' => '50002', 'debug' => '0', 'output' => 'xmlrpc'));

// getProjectList returns a string if it can't find any sub directories.
// otherwise an array is returned..
if (gettype($projList) == "string")
  exit("<h5>$projList</h5>");

// echo '<h5>PHP Native Results printed via print_r()</h5>';
// echo '<xmp>';
// echo print_r($projList);
// echo '</xmp>';
?>

<!-- ----------------------------------------------------------------------- -->
<!-- LYNX support...                                                         -->
<!--                                                                         -->
<!-- TODO - update the time once a second via an XMLRPC call?                -->
<!-- ----------------------------------------------------------------------- -->
<noframes>
Links to status pages...<br>
<?php foreach ($dsmList as $key => $val) { ?>
<a href="<?=$key?>.html"><?=str_pad($val,11, '_')?></a><br>
<?php } ?>
</noframes>

<!-- ----------------------------------------------------------------------- -->
<!-- This script causes the 'stat' iframe to display the status page of a    -->
<!-- selected DSM in the control_dsm form.                                   -->
<!-- ----------------------------------------------------------------------- -->
<script type='text/javascript'>
<!--//
function clicker(that) {
    var pick = that.options[that.selectedIndex].value;
<?php foreach ($dsmList as $key => $val) { ?>
    if (pick == '<?=$key?>') parent.stat.location='<?=$key?>.html';
<?php } ?>
<!--     if (pick == 'calibrate analog') <?=$calibrate='yes'?>; -->
}
//-->
</script>

<!-- ----------------------------------------------------------------------- -->
<!-- This form provides a selection of project, aircraft, and flight number  -->
<!-- to choose from for specifing a folder path to record data in.           -->
<!-- ----------------------------------------------------------------------- -->
<form action='project.php' method='POST' target=stat>

  record: project&nbsp
  <select name='project' size=1>
    <option value='' selected>
    <?php foreach ($projList as $project) { ?>
    <option><?=$project?></option>
    <?php } ?>
  </select>&nbsp

  aircraft&nbsp
  <!-- TODO obtain this as a list from the server as well... -->
  <select name='aircraft' size=1>
    <option value='' selected>
    <option>GV
    <option>C-130
  </select>&nbsp

  flight &#035&nbsp
  <!-- TODO obtain this as a list from the server as well... -->
  <input type=text name='flight' size=3 maxlen=3/>&nbsp
  <input type='submit' value='start'  class='button'/>
</form>
<hr align='center' width='100%'><p>

<!-- ----------------------------------------------------------------------- -->
<!-- This form provides a selection of DSMs to control.  There are two       -->
<!-- steps in this form: the selection of the dsm and the choice of action.  -->
<!--                                                                         -->
<!-- TODO - higlight the option red when in warning...                       -->
<!--  <option value='YYY'                        >YYY (----/--/-- --:--:--)  -->
<!--  <option value='XXX' style='color: #ff0000;'>YYY (----/--/-- --:--:--)  -->
<!--                                                                         -->
<!-- TODO - update the time once a second via an XMLRPC call?                -->
<!--  <option value='<?=$key?>'><?=str_pad($val,11, '_')?> (----/--/-- --:--:--)</option> -->
<!-- ----------------------------------------------------------------------- -->
<form name='control_dsm' action='control_dsm.php' method='POST' target=stat>

  <select name='dsm[]' onclick='clicker(this)' size="4" multiple="multiple">
    <?php foreach ($dsmList as $key => $val) { ?>
    <option value='<?=$key?>' id='<?=$key?>'></option>
    <?php } ?>
  </select><p>

  action:&nbsp
  <select name='act' onclick='clicker(this)'>
    <option value='' selected>
    <option>Start</option>
    <option>Stop</option>
    <option>Restart</option>
    <option>Quit</option>
<!--     <option>calibrate analog</option> -->
  </select>

  &nbsp<input type=submit value='submit &raquo;' class='button'/>
</form>

<!-- ----------------------------------------------------------------------- -->
<!-- This script displays the dsm[] items in the control_dsm form.           -->
<!-- ----------------------------------------------------------------------- -->
<script language="JavaScript1.2">
<!--//
<?php foreach ($dsmList as $key => $val) { ?>
document.getElementById('<?=$key?>').innerHTML="<?=str_pad($val,11, '_')?> (---- -- -- --:--:--)"
<?php } ?>
//-->
</script>
