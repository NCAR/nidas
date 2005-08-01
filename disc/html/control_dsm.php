<?php
include_once('utils/utils.php');

if (empty($_POST['dsm'])) {
  echo '<h5>You need to select some DSM(s)</h5>';
  exit;
}
$action = $_POST['act'];
if (empty($action)) {
  echo '<h5>You need to select an action</h5>';
  exit;
}
if ($action == 'calibrate analog') {
  echo '<h5>You wish!</h5>';
  exit;
}
if (sizeof($_POST['dsm']) > 1)
  echo '<h5>Results from the XMLRPC calls</h5>';
else
  echo '<h5>Results from the XMLRPC call</h5>';

foreach ($_POST['dsm'] as $dsm) {
  echo "$action $dsm... ";

  $result = xu_rpc_http_concise( array( 'method' => $action,
                                        'args' => '',
                                        'host' => $dsm,
       'uri' => '/RPC2', 'port' => '50003', 'debug' => '0', 'output' => 'xmlrpc'));

  if ($result)
    print_r($result);
  else
    echo "(no response)";
  echo "<br>";
}
?>
