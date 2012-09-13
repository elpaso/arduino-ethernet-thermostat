
function loadScript(url, callback){

    var script = document.createElement("script")
    script.type = "text/javascript";

    if (script.readyState){  //IE
        script.onreadystatechange = function(){
            if (script.readyState == "loaded" ||
                    script.readyState == "complete"){
                script.onreadystatechange = null;
                callback();
            }
        };
    } else {  //Others
        script.onload = function(){
            callback();
        };
    }

    script.src = url;
    document.getElementsByTagName("head")[0].appendChild(script);
}

var headID = document.getElementsByTagName("head")[0];

var newLink = document.createElement('link');
newLink.rel = "stylesheet";
newLink.href = "http://code.jquery.com/mobile/1.1.1/jquery.mobile-1.1.1.min.css";
headID.appendChild(newLink);

var newTitle = document.createElement('title');
newTitle.innerHTML="Controllo riscaldamento";
headID.appendChild(newTitle);


var newStyle = document.createElement('style');
newStyle.innerHTML +='table.program-table td {border: solid 1px white; text-align:center;}'
newStyle.innerHTML +='table.program-table th {border: solid 1px white; text-align:center;}'
newStyle.innerHTML +='table.program-table {border-collapse: collapse;}'
newStyle.innerHTML +='.T0 {background-color: #99FFFF;}'
newStyle.innerHTML +='.T1 {background-color: #FFFF33;}'
newStyle.innerHTML +='.T2 {background-color: #FF6600;}'
newStyle.innerHTML +='.T3 {background-color: #FF0000;text-shadow: none !important;}'
headID.appendChild(newStyle);


var newMeta = document.createElement('meta');
newMeta.name="viewport";
newMeta.content="width=device-width, initial-scale=1";
headID.appendChild(newMeta);

/* Commands */
var  CMD_ROOM_SET_PGM = 1;
var  CMD_WRITE_EEPROM = 2;
var  CMD_TIME_SET = 3;
var  CMD_TEMPERATURE_SET = 4;
var  CMD_RESET = 5;
var  CMD_W_PGM_SET_D_PGM =  6;
var  CMD_D_PGM_SET_T_PGM = 7;
var  CMD_SLOT_SET_UPPER_BOUND = 8;


function ws_call(cmd, parms){
    var url = "/?c="+cmd;
    if(parms.length){
        url += '&p=' + parms[0];
    }
    for(var i=1; i<parms.length; i++){
       url += '&v=' + parms[i];
    }
    $.getJSON( url ,
        function(data) {
            console.log(data);
    });
}

function set_time(){
    var d = new Date();
    // 0 = hh, 1 = mm, 2 = ss, 3 = Y, 4 = m, 5 = d
    ws_call(CMD_TIME_SET, [0, d.getHours()]);
    ws_call(CMD_TIME_SET, [1, d.getMinutes()]);
    ws_call(CMD_TIME_SET, [2, d.getSeconds()]);
}

function set_date(){
    var d = new Date();
    // 0 = hh, 1 = mm, 2 = ss, 3 = Y, 4 = m, 5 = d
    ws_call(CMD_TIME_SET, [3, d.getFullYear()]);
    ws_call(CMD_TIME_SET, [4, d.getMonth() + 1]);
    ws_call(CMD_TIME_SET, [5, d.getDate()]);
}

function change_program(pgm){
    ws_call(CMD_ROOM_SET_PGM, [window.location.hash.match(/room-(\d)-/)[1], pgm]);
}


// Global
var json_data = {};

var page_tpl = '\
<script id="room-tpl" type="text/x-jquery-tmpl">\
 <div id="room-${idx}-page" data-role="page" data-theme="b">\
    <div data-role="header">\
        <h1>${name}</h1>\
    </div>\
    <div data-role="content">\
        <h3 ><span class="t">${t}</span>°C</h3>\
        <p>Stato: <b class="s">${status}</b></p>\
        <p>Programma: <span class="p"><a class="ui-link" href="#program-${p}-page" data-rel="dialog">${program_name}</a></span> <a href="#program-dlg-page" data-role="button" data-inline="true" data-transition="fade" data-rel="dialog">Cambia</a></p>\
        <h3>desiderata: <span class="T">${T}</span>°C</h3>\
        <p><a href="#rooms-page" data-direction="reverse" data-role="button" data-rel="back"  data-icon="back">Indietro</a></p>\
    </div>\
  </div>\
</script>\
\
<script id="program-tpl" type="text/x-jquery-tmpl">\
 <div id="program-${pidx}-page" data-role="page" data-theme="b">\
    <div data-role="header">\
        <h1>Programma ${name}</h1>\
    </div>\
    <div data-role="content">\
        <table class="program-table">\
            <thead>\
                <tr><th>&nbsp;</th><th>0/{{each slot}}${value}</th><th id="slot-${sidx}">${value}/{{/each}}24</th></tr>\
            </thead>\
            <tbody id="program-${pidx}-tbody">\
            {{each day}}\
                <tr><th>${day_name}</th>{{each temperature}}<td id="PT-${pidx}${tidx}" class="T${tidx}">${value}°</td>{{/each}}</tr>\
            {{/each}}\
            </tbody>\
        </table>\
    </div>\
  </div>\
</script>\
\
<script id="program-dlg-tpl" type="text/x-jquery-tmpl">\
 <div id="program-dlg-page" data-role="page" data-theme="b">\
    <div data-role="header">\
        <h1>Scegli il nuovo programma</h1>\
    </div>\
    <div data-role="content">\
        <ul id="program-menu" data-role="listview" data-inset="true" data-filter="false">\
            {{each program}}<li><a href="javascript:change_program(${pidx})">${name}</a></li>{{/each}}\
        </ul>\
    </div>\
  </div>\
</script>\
\
<div id="home-page" data-role="page" data-theme="b">\
    <div data-role="header">\
        <h1>Riscaldamento</h1>\
    </div>\
    <div data-role="content">\
        <h2>Pompa centrale: <span id="pump-status"></span></h2>\
        <div id="blocked" style="display: none; color: red"><h2>Sistema bloccato!</h2>\
            <p>Sblocco alle: <b id="unlock-time"></b></p>\
        </div>\
        <h3>Data: <span id="data"></span> <a onclick="javascript:set_date()" data-role="button" data-inline="true" data-transition="fade" href="#">Sincronizza</a></h3><h3>Ora: <span id="ora"></span> <a onclick="javascript:set_time()" data-role="button" data-inline="true" data-transition="fade" href="#">Sincronizza</a></h2>\
        <ul id="main-menu" data-role="listview" data-inset="true" data-filter="false">\
            <li><a href="#rooms-page">Stanze</a></li>\
            <li><a href="#setup-page">Impostazioni</a></li>\
            <li><a href="#stats-page">Statistiche</a></li>\
        </ul>\
    </div>\
</div>\
<div id="rooms-page" data-role="page" data-theme="b">\
    <div data-role="header">\
        <h1>Controllo stanze</h1>\
    </div>\
    <div data-role="content">\
        <ul id="rooms-list" data-role="listview" data-inset="true" data-filter="false">\
        </ul>\
        <p><a href="#home-page" data-direction="reverse" data-role="button" data-rel="back"  data-icon="back">Home</a></p>\
    </div>\
</div>\
<div id="setup-page" data-role="page" data-theme="b">\
    <div data-role="header">\
        <h1>Impostazioni</h1>\
    </div>\
    <div data-role="content">\
        <div data-role="fieldcontain">\
            <label for="switch-a">Prova:</label><select name="switch-a" id="switch-a" data-role="slider">\
                <option value="off">Off</option>\
                <option value="on">On</option>\
            </select> \
        </div>\
        <div data-role="fieldcontain">\
            <label for="slider-T1">Slider:</label>\
            <input type="range" name="slider" id="slider-T2" value="5" min="5" max="25"  />\
        </div>\
        <div data-role="fieldcontain">\
            <label for="slider-T2">Slider:</label>\
            <input type="range" name="slider" id="slider-T2" value="5" min="5" max="25"  />\
        </div>\
        <div data-role="fieldcontain">\
            <label for="slider-T3">Slider:</label>\
            <input type="range" name="slider" id="slider-T3" value="5" min="5" max="25"  />\
        </div>\
        <p><a href="#home-page" data-direction="reverse" data-role="button" data-rel="back"  data-icon="back">Home</a></p>\
    </div>\
</div>\
';

document.getElementsByTagName("body")[0].innerHTML = page_tpl;

// Config
var room_names = [
  'Sala',
  'Bagno',
  'Camera 1',
  'Camera 2',
  'Camera 3'
];

var room_status = {
  'O': 'Riscaldamento in corso',
  'V': 'Apertura valvola',
  'B': 'Blocco',
  'C': 'Nessuna richiesta di calore'
};


var room_daily_program_names = [
    'Sempre T0 (antigelo)',
    'Sempre T1',
    'Sempre T2',
    'Sempre T3',
    'Risveglio, cena e sera',
    'Risveglio, pasti e sera',
    'Risveglio, pasti, pomeriggio e sera',
    'Tutto il giorno'
];

var day_names = ['Lun', 'Mar', 'Mer', 'Gio', 'Ven', 'Sab', 'Dom'];


var room_program_names = [
    room_daily_program_names[0],
    room_daily_program_names[1],
    room_daily_program_names[2],
    room_daily_program_names[3],
    'lun-ven: ' +  room_daily_program_names[4] + ' sab-dom: ' + room_daily_program_names[7],
    'lun-sab: ' +  room_daily_program_names[4] + ' dom: ' + room_daily_program_names[7],

    'lun-ven: ' +  room_daily_program_names[5] + ' sab-dom: ' + room_daily_program_names[7],
    'lun-sab: ' +  room_daily_program_names[5] + ' dom: ' + room_daily_program_names[7],

    'lun-ven: ' +  room_daily_program_names[6] + ' sab-dom: ' + room_daily_program_names[7],
    'lun-sab: ' +  room_daily_program_names[6] + ' dom: ' + room_daily_program_names[7],
]



function update_gui(){
    var data = new Date((json_data.status.u - 3600*1)*1000);
    $('#data').html(data.toLocaleDateString());
    $('#ora').html(data.toLocaleTimeString());
    $('#pump-status').html(json_data.status.P ? 'accesa' : 'spenta');
    if(json_data.status.b){
        $('#blocked').show();
        var unlock_data = new Date((json_data.status.b - 3600*1)*1000);
        //$('#unlock-date').html(unlock_data.toLocaleDateString());
        $('#unlock-time').html(unlock_data.toLocaleTimeString());
    } else {
        $('#blocked').hide();
    }
    $.each(json_data.status.R, function(k,v){
        v['idx'] = k;
        v['status'] = room_status[v.s];
        v['program_name'] = room_program_names[v.p];
        // Update elements
        $('#room-' + k + '-details').html(v.t + '° &mdash; desiderata: ' + v.T + '°' + (v.s == 'B' ? ' - In blocco' : '') );
        $('#room-' + k + '-page .s').html(v['status'] = room_status[v.s]);
        $('#room-' + k + '-page .t').html(v['status'] = v.t);
        $('#room-' + k + '-page .T').html(v['status'] = v.T);
        $('#room-' + k + '-page .p').html('<a class="ui-link" href="#program-' + v.p + '-page" data-rel="dialog">' +                         room_program_names[v.p] + '</a>');
        $('#room-' + k + '-page .d').html(room_daily_program_names[v.d]);
    });

    // Slot
    $.each(json_data.programs.s, function(k,v){
        $('slot-' + k).html(Math.floor(v/60) + ((v%60) ? ':' + (v%60) : ''));
    });

    $.each(json_data.programs.w, function(k,v){
        $.each(v, function(k,v){ // day
            $.each(json_data.programs.s, function(k1,v){
                $('PT-' + k + k1).html(json_data.programs.T[json_data.programs.d[v]]);
            });
        });
    });
}


loadScript('http://code.jquery.com/jquery-1.7.1.min.js', function(){
    loadScript('http://code.jquery.com/mobile/1.1.1/jquery.mobile-1.1.1.min.js', function(){
        loadScript('http://ajax.microsoft.com/ajax/jquery.templates/beta1/jquery.tmpl.min.js' , function(){
            $.getJSON( "/status" ,
                function(data) {
                    $.each(data.R, function(k,v){
                        $('#rooms-list').append('<li><a href="#room-' + k + '-page">' + room_names[k] + ' <span id="room-' + k + '-details"></span></a></li>');
                        v['name'] = room_names[k];
                        v['idx'] = k;
                        $( "#room-tpl" ).tmpl(v).appendTo(document.getElementsByTagName("body")[0]);
                    });
                    //$('#rooms-list').listview('refresh');

                    $.getJSON( "/programs" ,
                        function(data) {
                            json_data.programs = data;
                            var pgms = [];
                            var slot = [];
                            $.each(data.s, function(k,v){
                                slot.push({'sidx': k, 'value':Math.floor(v/60) + ((v%60) ? ':' + (v%60) : '')});
                            });
                            $.each(data.w, function(k,v){
                                var p = {'name':  'Programma ' + k, 'pidx': k, 'day': [], 'slot': slot };
                                $.each(v, function(k,v){ // day
                                    var temps = [];
                                    $.each(data.d[v], function(k,v){
                                        temps.push({'value': data.T[v], 'tidx': v});
                                    });
                                    p.day.push({'day_name': day_names[k], 'temperature' : temps});
                                });
                                pgms.push(p);
                            });
                            $( "#program-tpl" ).tmpl(pgms).appendTo(document.getElementsByTagName("body")[0]);
                            $( "#program-dlg-tpl" ).tmpl({program: pgms}).appendTo(document.getElementsByTagName("body")[0]);

                            (function worker() {
                            $.ajax({
                                url: '/status',
                                success: function(data) {
                                    json_data.status = data;
                                    update_gui();
                                },
                                complete: function() {
                                // Schedule the next request when the current one's complete
                                    setTimeout(worker, 2000);
                                }
                            });
                            })();
                        }
                    );


                }
            );
        });
    });
});

