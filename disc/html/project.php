<link href='index.css' rel='stylesheet' type='text/css'>

<?php
include_once('utils/utils.php');

$project = $_POST['project'];
if (empty($project)) {
  echo '<h5>You need to select a project!</h5>';
  exit;
}
$aircraft = $_POST['aircraft'];
if (empty($aircraft)) {
  echo '<h5>You need to select an aircraft!</h5>';
  exit;
}
$flight = trim($_POST['flight']);
if (empty($flight)) {
  echo '<h5>You need to enter a flight number!</h5>';
  exit;
}
echo '<h4>Results from the XMLRPC call to the DSM server:</h4>';

$result = xu_rpc_http_concise( array( 'method' => 'SetProject',
                                      'args' => array($project,$aircraft,$flight),
                                      'host' => 'localhost',
      'uri' => '/RPC2', 'port' => '50002', 'debug' => '0', 'output' => 'xmlrpc'));

if ($result)
  print_r($result);
else
  echo "(no response)";
echo "<br>";
?>
