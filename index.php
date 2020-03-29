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

$body .= "<div>\n";
$body .= "raw smoothed ";
$body .= "<span id='rawval_smoothed'>###</span>\n";
$body .= "</div>\n";

$body .= "<div>\n";
$body .= "grams ";
$body .= "<span id='grams'>###</span>\n";
$body .= "</div>\n";


$body .= "<div>\n";
$body .= "<button id='connect'>connect</button>\n";
$body .= "<button id='disconnect'>disconnect</button>\n";

$body .= "</div>\n";

$body .= "<div>\n";
$body .= "<button id='tare'>tare</button>\n";
$body .= "</div>\n";

$body .= "<div>\n";
$body .= "<button id='cal10'>cal 10g</button>\n";
$body .= "</div>\n";

pfinish ();
