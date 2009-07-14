<?php
include_once('utils/utils.php');

$mthd = $_POST['mthd'];
$args = '';

echo "<script>window.parent.";

If (empty($_POST['rcvr']))
  exit("recvResp('<h5>web page receiver not set</h5>')</script>");

echo $_POST['rcvr'] . "('";

If (empty($_POST['host']))
  exit("<h5>You need to select a host</h5>')</script>");

If (empty($_POST['mthd']))
  exit("<h5>You need to select an action</h5>')</script>");

if (is_array($_POST['host']))
  $hosts = $_POST['host'];
else
  $hosts = array($_POST['host']);

if (sizeof($_POST['host']) > 1)
  echo "<h4>Results from the XMLRPC calls</h4>";
else
  echo "<h4>Results from the XMLRPC call</h4>";

foreach ($hosts as $host)
  echo "<br>host: " . $host;
echo "<br>mthd: " . $_POST['mthd'];
echo "<br>rcvr: " . $_POST['rcvr'];
echo "<br>device: " . $_POST['device'];
echo "<br>channel: " . $_POST['channel'];
echo "<br>voltage: " . $_POST['voltage'];
echo "<br>";

if ($mthd == "TestVoltage") {
  if ( empty($_POST['voltage']) && ($_POST['voltage'] != "0") )
    exit("<h5>You need to select a voltage</h5>')</script>");

  $args = array(
     'device'  => $_POST['device'],
     'channel' => $_POST['channel'],
     'voltage' => $_POST['voltage']
  );
}

foreach ($hosts as $host) {

  $port = '30004';
  if ($host == "localhost")
    $port = '30003';

  echo "$port $mthd $host... ";

  $result = xu_rpc_http_concise( array( 'method' => $mthd,
                                        'args'   => $args,
                                        'host'   => $host,
                                        'uri'    => '/RPC2',
                                        'port'   => $port,
                                        'debug'  => '0',
                                        'output' => 'xmlrpc' ));

  if (empty($result))
    echo "(no response)";

  else if (is_string($result))
    echo $result;

  else if (is_array($result))
    echo "<h5>" . $result['faultString'] . "</h5>";

  else
    echo "unknown reponse type: "+gettype($result);

  echo "<br>";
}
echo "');</script>";
?>
