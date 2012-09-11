
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

var page_tpl = '\
<script id="room-tpl" type="text/x-jquery-tmpl">\
 <div id="room-${idx}-page" data-role="page" data-theme="b">\
    <div data-role="header">\
        <h1>${name}</h1>\
    </div>\
    <div data-role="content">\
        <h3 ><span class="t">${t}</span>°C</h3>\
        <p>Stato: <b class="s">${status}</b></p>\
        <p>Programma: <span class="p"><a class="ui-link" href="#program-${p}-page" data-rel="dialog">${program_name}</a></span></p>\
        <h3>desiderata: <span class="T">${T}</span>°C</h3>\
        <p><a href="#rooms-page" data-direction="reverse" data-role="button" data-rel="back"  data-icon="back">Indietro</a></p>\
    </div>\
  </div>\
</script>\
\
<script id="program-tpl" type="text/x-jquery-tmpl">\
 <div id="program-${idx}-page" data-role="page" data-theme="b">\
    <div data-role="header">\
        <h1>Programma ${name}</h1>\
    </div>\
    <div data-role="content">\
        <table class="program-table">\
            <thead>\
                <tr><th>&nbsp;</th><th>0-6:30</th><th>6:30-8</th><th>8-12</th><th>12-13</th><th>13-16</th><th>16-20 </th><th>20-22</th><th>22-24</th></tr>\
            </thead>\
            <tbody id="program-${idx}-tbody">\
            {{each day}}\
                <tr><th>${day_name}</th>{{each temperature}}<td class="T${idx}">${value}°</td>{{/each}}</tr>\
            {{/each}}\
            </tbody>\
        </table>\
    </div>\
  </div>\
</script>\
\
\
<div id="home-page" data-role="page" data-theme="b">\
    <div data-role="header">\
        <h1>Riscaldamento</h1>\
    </div>\
    <div data-role="content">\
        <h2>Pompa centrale: <span id="pump-status"></span></h2>\
        <h2 id="blocked" style="display: none" class="error">Sistema bloccato!</h2>\
        <h3>Data: <span id="data"></span></h2>\
        <h3>Ora: <span id="ora"></span></h2>\
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

var room_daily_program = [
    [    0,   0,    0,    0,    0,    0,    0,    0 ], // all T0
    [    1,   1,    1,    1,    1,    1,    1,    1 ], // all T1
    [    2,   2,    2,    2,    2,    2,    2,    2 ], // all T2
    [    3,   3,    3,    3,    3,    3,    3,    3 ], // all T3
    [    1,   3,    1,    1,    1,    3,    2,    1 ], // awakening supper and evening 4
    [    1,   3,    1,    3,    1,    3,    2,    1 ],  // awakening, meals and evening 5
    [    1,   3,    1,    3,    3,    3,    2,    1 ],  // awakening, meals, afternoon and evening 6
    [    1,   3,    3,    3,    3,    3,    2,    1 ]

];

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

var room_programs = [
        //  Mo Tu Th We Fr Sa Su
        [0, 0, 0, 0, 0, 0, 0], // always off
        [1, 1, 1, 1, 1, 1, 1], // Always 1
        [2, 2, 2, 2, 2, 2, 2], // Always 2
        [3, 3, 3, 3, 3, 3, 3], // Always 3
        [4, 4, 4, 4, 4, 7, 7], // 4 (5+2)
        [4, 4, 4, 4, 4, 4, 7], // 4 (6+1)
        [5, 5, 5, 5, 5, 7, 7], // 5 (5+2)
        [5, 5, 5, 5, 5, 5, 7], // 5 (6+1)
        [6, 6, 6, 6, 6, 7, 7], // 6 (5+2)
        [6, 6, 6, 6, 6, 6, 7]  // 6 (6+1)
]

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



function update_gui(json_data){
    var data = new Date((json_data.u - 3600*2)*1000);
    $('#data').html(data.toLocaleDateString());
    $('#ora').html(data.toLocaleTimeString());
    $('#pump-status').html(json_data.P ? 'accesa' : 'spenta');
    if(json_data.b){
        $('#blocked').show();
    } else {
        $('#blocked').hide();
    }
    $.each(json_data.R, function(k,v){
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
}


loadScript('http://code.jquery.com/jquery-1.7.1.min.js', function(){
    loadScript('http://code.jquery.com/mobile/1.1.1/jquery.mobile-1.1.1.min.js', function(){
        loadScript('http://ajax.microsoft.com/ajax/jquery.templates/beta1/jquery.tmpl.min.js' , function(){
            $.getJSON( "/status" ,
                function(data) {
                    $.each(data.R, function(k,v){
                        $('#rooms-list').append('<li><a href="#room-' + k + '-page">' + room_names[k] + ' <span id="room-' + k + '-details"></span></a></li>');
                        v['idx'] = k;
                        v['status'] = room_status[v.s];
                        v['name'] = room_names[k];
                        v['program_name'] = room_program_names[v.p];
                        $( "#room-tpl" ).tmpl(v).appendTo(document.getElementsByTagName("body")[0]);
                    });
                    //$('#rooms-list').listview('refresh');
                    update_gui(data);
                    var pgms = [];
                    $.each(room_programs, function(k,v){
                        var p = {'name':  room_program_names[k], 'idx': k, 'day': []};
                        $.each(v, function(k,v){ // day
                            var temps = [];
                            $.each(room_daily_program[v], function(k,v){
                                temps.push({'value': data.T[v], 'idx': v});
                            });
                            p.day.push({'day_name': day_names[k], 'temperature' : temps});
                        });
                        pgms.push(p);
                    });
                    $( "#program-tpl" ).tmpl(pgms).appendTo(document.getElementsByTagName("body")[0]);
                }
            );

            (function worker() {
            $.ajax({
                url: '/status',
                success: function(data) {
                    update_gui(data);
                },
                complete: function() {
                // Schedule the next request when the current one's complete
                    setTimeout(worker, 2000);
                }
            });
            })();
        });
    });
});

