#!/bin/php
<?php
header("HTTP/1.1 404 Not Found");
echo 'test';

for($i=0;$i < 3; $i++) {
echo 'test';
sleep(5);
}
exit;
