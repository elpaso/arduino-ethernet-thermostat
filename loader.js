
var headID = document.getElementsByTagName("head")[0];
var newScript = document.createElement('script');
newScript.type = 'text/javascript';
newScript.src = 'http://ajax.googleapis.com/ajax/libs/jquery/1.8.1/jquery.min.js';
headID.appendChild(newScript);
var newScript = document.createElement('script');
newScript.type = 'text/javascript';
newScript.src = 'http://ajax.googleapis.com/ajax/libs/jqueryui/1.8.23/jquery-ui.min.js';
headID.appendChild(newScript);
var newTitle = document.createElement('title');
document.getElementsByTagName('title')[0].innerHTML="Controllo riscaldamento";

