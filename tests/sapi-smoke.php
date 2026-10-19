<?php
// No connection or credentials: verify registration in the executing SAPI.
$available = extension_loaded('pdo_odbc')
    && in_array('odbc', PDO::getAvailableDrivers(), true)
    && in_array('sqlite', PDO::getAvailableDrivers(), true);
echo 'PDO_ODBC ', PHP_SAPI, ' ', $available ? 'OK' : 'FAIL', PHP_EOL;
exit($available ? 0 : 1);
