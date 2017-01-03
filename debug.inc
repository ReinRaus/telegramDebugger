<?php
namespace debug;
$debugTimeout = 600; // если страница отладки не создает запросы более 10 минут, то прекращаем отладку
const LOG = true; // сообщения в лог самого отладчика

if ( $_SERVER['HTTP_HOST']=="localhost" ) {
	header( "Access-Control-Allow-Origin: *" );
	error_reporting( E_ALL );
}
$debug = @$_GET["debug"];

class Debugger {
	function __construct( $debugTimeout ) {
		$this->storageName = __DIR__."/storage.ser";
		$this->debugTimeout = $debugTimeout;
		$this->loadStorage();
	}
	/** показываем страницу отладчика */
	function showManagerPage(){
		if (LOG) log( "Показываем страницу отладчика");
		header( "Access-Control-Allow-Origin: *" );
		die( file_get_contents( __DIR__."/index.html" ) );
	}
	/** возвращаем чаты на которых запущена отладка */
	function getDebugChats(){
		$chats = $this->storage["chats"];
		if ( !isset( $chats ) ) die( "" );
		else die( implode( ";", $chats ) );
	}
	/**  запускаем отладку для указанных чатов */
	function setDebugChats( $chats ){
		if (LOG) log( "Установка чатов для отладки: $chats");
		$this->storage["chats"] = explode( ";", $chats );
		$this->storage["running"] = time();
		$this->storage["activeChats"] = Array();
		$this->storage["requests"] = Array();
		$this->writeStorage();
	}
	/** возвращаем чаты в которых что-то писали с момента запуска отладки (посмотреть свой ID) */
	function getActiveChats() {
		if ( !$this->debugSessionActive() ) die( "Отладка не запущена" );
		if ( count( $this->storage["activeChats"] )==0 ) die ( "Нет активных чатов с момента запуска" );
		foreach ( $this->storage["activeChats"] as $k=>$v ) {
			echo $v;
			$id = preg_replace( "/^(\d+).*$/s", "$1", $v );
			if ( in_array( $id, $this->storage["chats"] ) ) echo " [отладка]\n";
		}
		die();
	}
	/** Извлекаем запрос из очереди, если он есть, иначе- спим */
	function acceptLongPooling(){
		if (LOG) log( "Начат длинный запрос");
		$this->storage["running"] = time();
		$this->writeStorage();
		set_time_limit( 120 ); // ждем не более двух минут
		while ( sizeof($this->storage["requests"]) < 1) {
			usleep( 100000 ); // 100 ms
			$this->loadStorage();
			if ( !isset($this->storage["requests"]) ) $this->storage["requests"]=Array(); // иначе куча нотисов в логе Apache
		}
		$request = array_shift( $this->storage["requests"] );
		if (LOG) log( "Что-то нашлось для ответа на длинный запрос ($request)");
		$this->writeStorage();
		die( $request );
	}
	/** Получили запрос на отладку - ставим его в очередь */
	function startProxyMessage( $message ) {
		if (LOG) {
			try {
				$text = json_decode($message, true)['message']['text'];
			} catch (Exception $e ){
				$text = "...";
			};
			log( "Поступило новое сообщение для очереди ($text)");
		}
		$this->storage["requests"][] = $message;
		$this->writeStorage();
		die();
	}
	/** проверка, что находимся в режиме отладки */
	function debugSessionActive(){
		return (time() - $this->storage["running"] ) < $this->debugTimeout;
	}
	/** Получить весь массив с данными отладки */
	function loadStorage() {
		$result = @unserialize( file_get_contents( $this->storageName ) );
		if ( !isset( $result ) ) $result = Array();
		$this->storage = $result;
	}
	/** Сохранить весь массив с данными отладки*/
	function writeStorage() {
		file_put_contents( $this->storageName, serialize( $this->storage ), LOCK_EX );
	}
	function showLog(){
		die( "<PRE>".@file_get_contents( __DIR__.'/log.txt' ).'</PRE>' );
	}
	function clearLog(){
		unlink( __DIR__.'/log.txt' );
	}
}

function log( $text ) {
	file_put_contents( __DIR__.'/log.txt', date( DATE_RSS )."	$text	[$_SERVER[REQUEST_TIME_FLOAT]]\n", LOCK_EX|FILE_APPEND );
}

$debugger = new Debugger( $debugTimeout );
if ( $debug == "manage" ) $debugger->showManagerPage();
else if ( $debug == "getDebugChats" ) $debugger->getDebugChats();
else if ( $debug == "start" ) $debugger->setDebugChats( $_POST["chats"] );
else if ( $debug == "getActiveChats" ) $debugger->getActiveChats();
else if ( $debug == "longPooling" ) $debugger->acceptLongPooling();
else if ( $debug == "showLog" ) $debugger->showLog();
else if ( $debug == "clearLog" ) $debugger->clearLog();

try {
	$text= file_get_contents( "php://input" );
	$update = json_decode( $text, true );
	$chatId = $update['message']['chat']['id'];
	$chatName = $update['message']['chat']['title'].$update['message']['chat']['username'];
	if ( !isset( $debugger->storage["activeChats"][ $chatId ])) {
		$value = $chatId." : ".$chatName;
		if ( !in_array( $value, $debugger->storage["activeChats"] ) && ($chatId !="" || $chatName != "" ) ) {
			if (LOG) log( "Новый активный чат $value" );
			$debugger->storage["activeChats"][] = $value;
			$debugger->writeStorage();
		}
	}
} catch (Exception $e){};

if ( $debugger->debugSessionActive() && !$debug) {
	if ( in_array( $chatId, $debugger->storage["chats"] ) ) $debugger->startProxyMessage( $text );
}
