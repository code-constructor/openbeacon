<?php
    error_reporting(E_ALL);
    ini_set("display_errors", 1);

    if(isset($_GET['floor']))
    {
	$floor=intval($_GET['floor']);
	if($floor<1)
	    $floor=1;
	else
	    if($floor>3)
		$floor=3;
    }
    else
	$floor=1;

    $time_start = microtime(TRUE);

    $im = imagecreatefrompng($floor.'og768x546.png')
	or die('Cannot Initialize new GD image stream');

    $im_width = imagesx($im);
    $im_height = imagesy($im);

    $readers = array(
	1 => array (
	   
	),
	2 => array (
	    array(112, 5, 5),
	    array(113, 75, 5),
	    array(114, 5, 91),
	    array(115, 75, 91),
	    
	),
	3 => array (
	    
	)
    );

    $background_color = imagecolorallocate($im, 200, 200, 200);
    $wall_color = imagecolorallocate($im, 215, 215, 215);
    $inactive_color = imagecolorallocate($im, 128, 128, 128);
    $text_color = imagecolorallocate($im,  64, 128, 32);
    $reader_color = imagecolorallocate($im, 255, 64, 32);
    $tag_color = imagecolorallocate($im, 32, 64, 255);

    foreach($readers[$floor] as $reader)
    {
	$x=$reader[1];
	$y=$reader[2];

	imagefilledrectangle($im, $x-3, $y-3, $x+3, $y+3, $reader_color);
	imagestring($im, 4, $x, $y+5, $reader[0], $inactive_color);
    }

    // timestamp
    $duration = ' ['.round((microtime(TRUE)-$time_start)*1000,1).'ms]';
    imagestring($im, 4, $im_width-330, $im_height-30, date(DATE_RSS).$duration, $inactive_color);

    $link = mysql_connect('localhost', 'opentracker', 'HYVbrwTP6JMuBL2w');
    if ($link && mysql_select_db('opentracker', $link))
    {
	$result = mysql_query('
	    SELECT tag_id, x, y
	    FROM distance
	    WHERE TIMESTAMPDIFF(SECOND,last_seen,NOW())<5
	');
	$tag = array();
	while ($row = mysql_fetch_assoc($result))
	    $tag[intval($row['tag_id'])]=array( 'x'=>$row['x'], 'y'=>$row['y'] );

	$result = mysql_query(' 
	    SELECT tag_id, proximity_tag
	    FROM proximity
	    WHERE TIMESTAMPDIFF(SECOND,time,NOW())<30 
	');

	while ($row = mysql_fetch_assoc($result))
	    $tag[intval($row['tag_id'])][proximity]=intval($row['proximity_tag']);


	foreach( $tag as $id => $position )
	{
	    $x = $position['x'];
	    $y = $position['y'];
	    imagefilledellipse($im, $x, $y, 6, 6, $tag_color);
	    imagestring($im, 4, $x,$y+6, $id."[".$position['x'].",".$position['y']."]", $text_color);
	    if(isset($position['proximity'])&&(($prox = intval($position['proximity']))!=0))
	    {
		$x2 = $tag[$prox]['x'];
		$y2 = $tag[$prox]['y'];
		imageline($im,$x,$y,$x2,$y2,$inactive_color);
	    }
	}
    }

    header('Content-type: image/png');
    header('Refresh: 1');
    imagepng($im);
    imagedestroy($im);
?>
