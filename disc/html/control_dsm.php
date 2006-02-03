<?php
include_once('utils/utils.php');

echo "<script>window.parent.selectedDsm=''; window.parent.recvResp('";

If (empty($_POST['dsm']))
  exit("<h5>You need to select some DSM(s)</h5>')</script>");

$action = $_POST['act'];
if (empty($action))
  exit("<h5>You need to select an action</h5>')</script>");

if (sizeof($_POST['dsm']) > 1)
  echo "<h4>Results from the XMLRPC calls</h4>";
else
  echo "<h4>Results from the XMLRPC call</h4>";

foreach ($_POST['dsm'] as $dsm) {
  echo "$action $dsm... ";

  $result = xu_rpc_http_concise( array( 'method' => $action,
                                        'args'   => '',
                                        'host'   => $dsm,
                                        'uri'    => '/RPC2',
                                        'port'   => '30004',
                                        'debug'  => '0',
                                        'output' => 'xmlrpc' ));

  if (empty($result))
    echo "(no response)";

  else if (gettype($result) == "string")
    echo $result;

  else if (gettype($result) == "array")
    echo $result['faultString'];

  else
    echo "unknown reponse type: "+gettype($result);

  echo "<br>";
}
echo "');</script>";
?>
