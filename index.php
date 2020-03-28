<?php

require_once("app.php");

$anon_ok = 1;

pstart ();

$body .= "<div>\n";
$body .= mklink ("home", "/");
$body .= "</div>\n";

$body .= "<p>hello</p>\n";

$body .= "<div>\n";
$body .= "raw val ";
$body .= "<span id='rawval'>###</span>\n";
$body .= "</div>\n";


$body .= "<button id='button1'>button1</button>\n";

pfinish ();
