<?php
ini_set('display_errors', 1);
ini_set('display_startup_errors', 1);
error_reporting(E_ALL);

if(!empty($_REQUEST['token']) && $_REQUEST['token'] === '') {
	$url = 'https://eu-wap.tplinkcloud.com?token=';
	$data = [
		'method' => 'passthrough',
		'params' => [
			"deviceId" => "",
			"requestData" => "{\"system\":{\"set_relay_state\":{\"state\":" . (!empty($_REQUEST['state']) ? $_REQUEST['state'] : 0) . "}}}",
		],
	];
	$curl = curl_init($url);
    curl_setopt($curl, CURLOPT_POST, true);
    curl_setopt($curl, CURLOPT_POSTFIELDS, json_encode($data));
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, true);
    curl_setopt($curl, CURLOPT_HTTPHEADER, ['Content-Type: application/json']);
    $response = curl_exec($curl);
    curl_close($curl);
    print_r(json_encode([$data, json_decode($response)]));
}
