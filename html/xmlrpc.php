<?php
$method = $args = null;

/// GET systax
if (!empty($_GET['port']))   $port   = $_GET['port'];
if (!empty($_GET['method'])) $method = $_GET['method'];
if (!empty($_GET['args']))   $args   = $_GET['args'];

// /// POST systax (needs work...)
// global $HTTP_RAW_POST_DATA;
// if (!$HTTP_RAW_POST_DATA) {
//    exit('XML-RPC server accepts POST requests only.');
// }
// $data = $HTTP_RAW_POST_DATA;
// exit("$data");

include_once('utils/utils.php');

$response = xu_rpc_http_concise(
   array( 'method'   => $method,
          'args'     => $args,
          'host'     => 'localhost',
          'uri'      => '/RPC2',
          'port'     => $port,
          'debug'    => '0',
          'output'   => 'xmlrpc',
          'nodecode' => 'true',
          'timeout'  => '0'  // seconds (0 = never)
          )
   );
exit("$response");
?>
