<?php
namespace debug;
$debugTimeout = 300; // если страница отладки не создает запросы более 5 минут, то прекращаем отладку

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
		die( implode( "\n", $this->storage["activeChats"] ) );
	}
	/** Извлекаем запрос из очереди, если он есть, иначе- спим */
	function acceptLongPooling(){
		$this->storage["running"] = time();
		$this->writeStorage();
		set_time_limit( 120 ); // ждем не более двух минут
		while ( sizeof($this->storage["requests"]) < 1) {
			time_nanosleep( 0, 50000 ); // 50 ms
			$this->loadStorage();
		}
		$request = array_shift( $this->storage["requests"] );
		$this->writeStorage();
		die( $request );
	}
	/** Получили запрос на отладку - ставим его в очередь */
	function startProxyMessage( $message ) {
		$this->storage["requests"][] = $message;
		$this->writeStorage();
		die();
	}
	/** проверка, что находимся в режиме отладки */
	function debugSessionActive(){
		return time() - $this->storage["running"] < $this->debugTimeout;
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
}

$debugger = new Debugger( $debugTimeout );
if ( $debug == "manage" ) $debugger->showManagerPage();
else if ( $debug == "getDebugChats" ) $debugger->getDebugChats();
else if ( $debug == "start" ) $debugger->setDebugChats( $_POST["chats"] );
else if ( $debug == "getActiveChats" ) $debugger->getActiveChats();
else if ( $debug == "longPooling" ) $debugger->acceptLongPooling();

if ( $debugger->debugSessionActive() && !$debug) {
	$text= file_get_contents( "php://input" );
	$update = json_decode( $text, true );
	$chatId = $update['message']['chat']['id'];
	$chatName = $update['message']['chat']['title'].$update['message']['chat']['username'];
	if ( !isset( $debugger->storage["activeChats"][ $chatId ])) {
		$value = $chatId." : ".$chatName;
		if ( !in_array( $value, $debugger->storage["activeChats"] ) ) {
			$debugger->storage["activeChats"][] = $value;
			$debugger->writeStorage();
		}
	}
	if ( in_array( $chatId, $debugger->storage["chats"] ) ) $debugger->startProxyMessage( $text );
}
