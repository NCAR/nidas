<link href='index.css' rel='stylesheet' type='text/css'>

<!-- ----------------------------------------------------------------------- -->
<!-- Query the dsm_server for a list of DSM names and locations.             -->
<!-- ----------------------------------------------------------------------- -->
<?php
include_once('utils/utils.php');

$dsmList = xu_rpc_http_concise( array( 'method' => 'getDsmList',
                                       'args' => '',
                                       'host' => '127.0.0.1',
      'uri' => '/RPC2', 'port' => '50002', 'debug' => '0', 'output' => 'xmlrpc'));

// echo '<h5>PHP Native Results printed via print_r()</h5>';
// echo '<xmp>';
// echo print_r($dsmList);
// echo '</xmp>';

if (empty($dsmList)) {
  exit('<h5>Cannot receive list of DSMs from the server!</h5>');
  exit;
}
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
<!-- This form provides the fight number to the dsm_server.  This number is  -->
<!-- used to specify a folder path to record data in.                        -->
<!-- ----------------------------------------------------------------------- -->
<form name='flight_num' action='flightnum.php' method='GET'>
  record flight number&nbsp 
  <input type=text name='flight_num' size=3 maxlen=3/>&nbsp
  <input type='submit' value='start' class='button'/>&nbsp
  <input type='submit' value='stop'  class='button'/>
</form>
<hr align='center' width='100%'><p>


<!-- ----------------------------------------------------------------------- -->
<!-- This form provides a selection of DSMs to control.  There are two       -->
<!-- steps in this form: the selection of the dsm and the choice or action.  -->
<!--                                                                         -->
<!-- TODO - higlight the option red when in warning...                       -->
<!--  <option value='YYY'                        >YYY (----/--/-- --:--:--)  -->
<!--  <option value='XXX' style='color: #ff0000;'>YYY (----/--/-- --:--:--)  -->
<!--                                                                         -->
<!-- TODO - update the time once a second via an XMLRPC call?                -->
<!--  <option value='<?=$key?>'><?=str_pad($val,11, '_')?> (----/--/-- --:--:--)</option> -->
<!-- ----------------------------------------------------------------------- -->
<form name='control_dsm' action='control_dsm.php' method='GET' target=stat>

  <select name='dsm[]' onclick='clicker(this)' multiple>
    <?php foreach ($dsmList as $key => $val) { ?>
    <option value='<?=$key?>' id='<?=$key?>'></option>
    <script language="JavaScript1.2">
    <!--//
    document.getElementById('<?=$key?>').innerHTML="<?=str_pad($val,11, '_')?> (---- -- -- --:--:--)"
    //-->
    </script>
    <?php } ?>
  </select><p>

  action:&nbsp
  <select name='act' onclick='clicker(this)'>
    <option value='nothing' selected>
    <option>start</option>
    <option>stop</option>
    <option>restart</option>
    <option>quit</option>
<!--     <option>calibrate analog</option> -->
  </select>

  &nbsp<input type=submit value='submit &raquo;' class='button'/>
</form>
