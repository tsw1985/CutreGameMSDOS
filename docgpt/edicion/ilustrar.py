"""Create vector teaching figures. Only writes inside docgpt/figuras."""
from pathlib import Path
from html import escape
import json
import math

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'figuras'
OUT.mkdir(exist_ok=True)
NAVY, TEAL, CORAL, GOLD, INK = '#173548', '#148f82', '#d45d46', '#d7a33c', '#243746'
PALE, LINE, WHITE = '#eef5f4', '#c5d6dc', '#ffffff'
REGISTRY = {}

def txt(x, y, text, size=21, color=INK, anchor='start', weight='normal'):
    return f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" text-anchor="{anchor}" font-weight="{weight}">{escape(str(text))}</text>'

def rect(x, y, w, h, fill=PALE, stroke='none', radius=9, extra=''):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{radius}" fill="{fill}" stroke="{stroke}" stroke-width="2" {extra}/>'

def line(x1, y1, x2, y2, color=INK, width=2, dash=''):
    return f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" stroke-width="{width}" stroke-dasharray="{dash}"/>'

def arrow(x1, y1, x2, y2, color=TEAL):
    return f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{color}" stroke-width="3" marker-end="url(#arrow)"/>'

def box(x, y, w, h, title, sub='', fill=PALE):
    s = rect(x,y,w,h,fill)
    s += txt(x+w/2,y+36,title,23,anchor='middle',weight='bold')
    if sub:
        for i, row in enumerate(sub.split('\n')):
            s += txt(x+w/2,y+68+i*26,row,18,anchor='middle')
    return s

def tank(x,y,scale=1,color=TEAL):
    return (rect(x,y,4*scale,18*scale,NAVY,radius=scale)
            +rect(x+14*scale,y,4*scale,18*scale,NAVY,radius=scale)
            +rect(x+4*scale,y+3*scale,10*scale,13*scale,color,radius=scale)
            +rect(x+8*scale,y,2*scale,10*scale,color,radius=0))

def save(name, title, elements, caption, chapter=None, height=500):
    content = ''.join(elements)
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="1040" height="{height}" viewBox="0 0 1040 {height}" role="img" aria-labelledby="title desc">
<title id="title">{escape(title)}</title><desc id="desc">{escape(caption)}</desc>
<defs><marker id="arrow" markerWidth="9" markerHeight="9" refX="8" refY="4.5" orient="auto"><path d="M0,0 L9,4.5 L0,9" fill="{TEAL}"/></marker></defs>
<rect width="1040" height="{height}" rx="16" fill="#f7faf9"/>
<g font-family="DejaVu Sans, sans-serif">{txt(28,43,title,26,NAVY,weight='bold')}{content}</g></svg>'''
    (OUT/(name+'.svg')).write_text(svg)
    REGISTRY[name] = {'file': 'figuras/'+name+'.svg', 'title': title, 'caption':caption, 'chapter':chapter}

# Architecture and basic language.
save('sistema','Una partida: datos, reglas y presentación',[
    box(30,100,260,110,'Entradas','teclado + IPX'), box(390,100,260,110,'Simulación','estado + paredes'),
    box(750,100,260,110,'Presentación','cámara + sprites'), arrow(295,155,382,155),arrow(655,155,742,155),
    box(390,290,260,110,'Eventos de audio','mezcla → DMA'),arrow(520,215,520,280),
    txt(45,450,'La cámara consulta el mundo. Las reglas no dependen de lo que se ve.',23)
], 'Las entradas cambian el estado compartido; imagen y sonido se derivan de ese estado.', 'ch00.md')
save('construccion','Del archivo C al juego que lee recursos',[
    box(35,110,260,120,'Fuentes .c / .asm','instrucciones legibles'),
    box(390,110,260,120,'Objetos .obj','código de cada módulo'),
    box(745,110,260,120,'bin/game.exe','programa enlazado'),arrow(300,170,380,170),arrow(655,170,735,170),
    txt(336,100,'compilar',18,anchor='middle'),txt(696,100,'enlazar',18,anchor='middle'),
    box(390,320,260,100,'res: BMP y WAV','datos leídos al ejecutar'),arrow(655,368,870,240),
    txt(45,465,'El directorio actual determina cómo se resuelven las rutas relativas.',21)
], 'Los recursos se leen en ejecución; no se convierten en objetos durante el enlace.', 'ch01.md')
save('variable','Un bucle transforma estado paso a paso',[
    box(50,110,180,110,'x = 10','estado inicial'),arrow(240,165,310,165),
    box(320,110,180,110,'x = x + 2','primera vuelta'),arrow(510,165,580,165),
    box(590,110,180,110,'x = 12','nuevo estado'),
    txt(80,310,'Tras 8 vueltas:',27),txt(390,310,'10 + 8 × 2 = 26',32,TEAL,weight='bold'),
    txt(80,380,'Asignación: guardar un valor. Comparación: formular una pregunta.',22),
    txt(80,425,'El orden y el número de repeticiones forman parte del resultado.',22)
], 'El estado siguiente se calcula usando el estado actual, no la posición que imaginas en pantalla.', 'ch02.md')
e=[txt(40,105,'Imagen de 8 × 3: una sola secuencia de 24 bytes',23)]
for y in range(3):
    for x in range(8):
        e += [rect(60+x*110,140+y*72,95,58,TEAL if (x,y)==(3,2) else '#dce9ed'),txt(107+x*110,178+y*72,y*8+x,23,WHITE if (x,y)==(3,2) else INK,anchor='middle')]
e += [txt(60,425,'(x=3, y=2) → 2 × 8 + 3 = byte 19',28,TEAL,weight='bold')]
save('array','Dos coordenadas, un índice lineal',e,'La fila multiplica la anchura; la columna desplaza dentro de esa fila.', 'ch03.md')
e=[box(35,120,260,130,'Píxel: índice 37','un byte de memoria'),box(395,120,260,130,'Paleta: entrada 37','R=40, G=12, B=4'),box(755,120,250,130,'Color visible','interpretado por VGA'),arrow(300,185,385,185),arrow(660,185,745,185),rect(800,275,160,80,'#a23010'),txt(60,390,'320 × 200 = 64000 índices; 256 × 3 = 768 componentes de paleta.',23),txt(60,440,'Cambiar la entrada 37 cambia todos los píxeles que usan ese índice.',22)]
save('paleta','Un índice no es un color RGB',e,'En modo 13h, imagen y paleta son recursos distintos.', 'ch04.md')
e=[txt(40,105,'BMP de 320 × 200 usado por el proyecto',23)]
for x,w,title,sub,col in [(40,200,'54 bytes','cabeceras',GOLD),(250,270,'1024 bytes','256 entradas BGR0',TEAL),(530,470,'64000 bytes','píxeles + filas invertidas',NAVY)]:
    e += [rect(x,145,w,120,col),txt(x+w/2,192,title,26,WHITE,anchor='middle',weight='bold'),txt(x+w/2,230,sub,18,WHITE,anchor='middle')]
e += [txt(40,310,'0',20),txt(250,310,'54',20),txt(530,310,'1078',20),txt(995,310,'65078',20,anchor='end'),txt(50,390,'Archivo: BGR0 (4 bytes/entrada) → memoria DAC: 3 bytes/entrada.',23),txt(50,435,'Offset fijo 1078: contrato de estos recursos, no de cualquier BMP.',22)]
save('bmp','Qué hay antes del primer píxel',e,'La paleta del archivo ocupa 1024 bytes; la paleta convertida para el DAC ocupa 768.', 'ch05.md')
save('buffer','Construir en RAM, presentar en VGA',[
    box(40,115,250,120,'Fondo visible','200 filas × 320 bytes'),arrow(295,175,355,175),box(365,115,300,120,'Buffer de pantalla','fondo + sprites + HUD'),arrow(670,175,735,175),box(745,115,255,120,'VGA A000:0000','64000 bytes'),
    txt(380,280,'↑ tanques, balas, cifras',22,TEAL),txt(60,380,'La siguiente imagen se compone sin borrar delante del jugador.',23),txt(60,425,'El retrazo ayuda a presentar; no garantiza que cualquier copia quepa en él.',21)
], 'El buffer de composición permanece de 320 × 200 incluso al ampliar el mundo.', 'ch06.md')
e=[txt(40,100,'Índice 0: no escribir',23),txt(540,100,'Resultado sobre fondo',23)]
pattern=[[0,0,1,0,0],[1,1,1,1,1],[1,1,1,1,1],[1,1,1,1,1],[1,0,0,0,1]]
for y,row in enumerate(pattern):
    for x,v in enumerate(row):
        e += [rect(60+x*64,140+y*50,59,45,TEAL if v else '#e1e7e7',radius=2),txt(89+x*64,170+y*50,v,20,WHITE if v else INK,anchor='middle'),rect(590+x*64,140+y*50,59,45,TEAL if v else GOLD,radius=2)]
e += [arrow(400,255,540,255),txt(50,450,'La transparencia decide si se escribe; no mueve ni mezcla píxeles.',23)]
save('sprite','Componer un sprite transparente',e,'Ejemplo esquemático de transparencia; el tanque real mide 18 × 18.', 'ch07.md')
e=[txt(40,110,'Cinco bits describen una orden; varias teclas pueden estar activas.',23)]
for i,(key,val) in enumerate([('UP','0x01'),('DOWN','0x02'),('LEFT','0x04'),('RIGHT','0x08'),('FIRE','0x10')]):
    e += [box(35+i*202,160,186,115,key,val, '#d4efea' if i in (0,4) else PALE)]
e += [txt(80,355,'UP | FIRE = 0x01 | 0x10 = 0x11',31,TEAL,weight='bold'),txt(80,420,'La prioridad de movimiento se decide después de leer la combinación.',22)]
save('entrada','De teclas mantenidas a una máscara de órdenes',e,'El mismo byte de entrada puede obtenerse del teclado o recibirse por red.', 'ch08.md')
save('estado','Estado persistente y cambios de orientación',[
    box(35,110,280,150,'Tanque','posición (x,y)\ndirección + pose'),box(380,110,280,150,'Orden de avance','calcular candidato\nconsultar obstáculos'),box(725,110,280,150,'Estado siguiente','aceptar o conservar\nanimar según la regla'),arrow(320,180,370,180),arrow(665,180,715,180),txt(60,365,'El dibujo lee el estado; no decide si el movimiento fue válido.',25),txt(60,425,'Orientación, pose de animación y posición son datos diferentes.',23)
], 'Una pose visual no sustituye la posición ni la orientación lógica del tanque.', 'ch09.md')
e=[rect(75,100,400,320,'#e2ede8'),rect(90,115,370,45,NAVY),tank(200,180,12),txt(540,125,'Movimiento candidato hacia arriba',24,weight='bold')]
for i,(x,y,label) in enumerate([(106,99,'Cañón: (106,99)'),(102,103,'Oruga izquierda: (102,103)'),(114,103,'Oruga derecha: (114,103)')]):
    px=200+(x-100)*12; py=180+(y-98)*12
    e += [f'<circle cx="{px}" cy="{py}" r="7" fill="{CORAL}"/>',txt(550,200+i*55,label,22)]
e += [txt(70,460,'Tres sondas delanteras: aproximación de colisión, no prueba de toda la silueta.',21)]
save('sondas','Consultar paredes en coordenadas de mundo',e,'Ejemplo de movimiento desde (100,100) a (100,98), con los puntos que comprueba el tanque.', 'ch10.md')
save('dos','Unas reglas, dos fuentes de entrada',[
    box(45,100,260,120,'Jugador 1','flechas + disparo'),box(45,300,260,120,'Jugador 2','WASD + disparo'),
    box(440,190,300,145,'Aplicar reglas','paredes + tanque rival\nposición + dirección'),arrow(310,160,430,230),arrow(310,360,430,290),
    txt(785,265,'MUNDO',27,TEAL,weight='bold'),txt(80,465,'La colisión entre tanques usa una caja interior; las balas usan otro criterio.',21)
], 'Las entradas se normalizan para aplicar las mismas reglas a ambos jugadores.', 'ch11.md')
e=[txt(35,105,'Estado de FIRE por frame',23)]
for i,value in enumerate([0,1,1,0,1]):
    e += [rect(45+i*190,145,170,85,TEAL if value else '#e0eaed'),txt(130+i*190,198,value,30,WHITE if value else INK,anchor='middle'),txt(130+i*190,270,'flanco' if i in (1,4) else '—',21,CORAL if i in (1,4) else INK,anchor='middle')]
e += [txt(55,350,'Un flanco pide disparar. Una bala ya activa puede impedir el nuevo disparo.',22),txt(55,420,'Pulsación mantenida ≠ una bala nueva por cada frame.',27,TEAL,weight='bold')]
save('disparo','Detectar la transición de libre a pulsada',e,'La secuencia tiene dos flancos de subida, pero solo uno se acepta si la primera bala sigue activa.', 'ch12.md')
save('ronda','Una ronda también tiene estado',[
    box(30,150,210,115,'Combate','movimiento + balas'),box(285,150,210,115,'Impacto','marcar explosión'),box(540,150,210,115,'Pausa','35 actualizaciones'),box(795,150,210,115,'Reinicio','salidas + cámara'),
    arrow(245,207,275,207),arrow(500,207,530,207),arrow(755,207,785,207),txt(70,360,'La puntuación persiste. Las balas y los estados temporales se restablecen.',23),txt(70,425,'El fundido del bucle oculta el salto entre el final y el nuevo comienzo.',23)
], 'El reinicio debe distinguir datos de ronda de la puntuación acumulada.', 'ch13.md')
e=[txt(45,110,'Un bloque DMA de 4096 bytes',25,weight='bold'),rect(45,145,460,95,TEAL),rect(505,145,460,95,NAVY),txt(275,200,'Mitad 0 · 2048 bytes',25,WHITE,anchor='middle'),txt(735,200,'Mitad 1 · 2048 bytes',25,WHITE,anchor='middle'),arrow(60,325,970,325)]
for x,t,action in [(65,'0 ms','lee mitad 0'),(505,'46,44 ms','IRQ → lee mitad 1'),(950,'92,88 ms','vuelve a mitad 0')]:
    e += [line(x,310,x,340),txt(x,285,t,22,anchor='middle' if x>100 else 'start'),txt(x,375,action,18,anchor='end' if x>900 else 'start')]
e += [txt(70,450,'Tras el aviso, rellena la mitad terminada mientras la tarjeta lee la otra.',23)]
save('dma','Dos mitades para producir y consumir audio',e,'A 44100 muestras por segundo, una mitad de 2048 muestras dura aproximadamente 46,44 ms.', 'ch14.md')
save('mezcla','Mezclar amplitudes y volver a unsigned',[
    box(40,115,250,130,'Muestra A = 228','228 − 128 = +100'),box(40,285,250,130,'Muestra B = 208','208 − 128 = +80'),
    box(420,190,240,130,'Suma = +180','limitar a +127'),arrow(295,180,410,230),arrow(295,350,410,270),
    box(755,190,250,130,'Salida = 255','127 + 128'),arrow(665,255,745,255),txt(65,465,'Con ambas voces a media ganancia: 50 + 40 = 90 → byte 218, sin recorte.',22)
], 'Primero se centra y escala cada voz; después se suma, satura y devuelve el desplazamiento de 128.', 'ch15.md')
save('stream','Dos buffers, dos plazos diferentes',[
    box(35,120,265,130,'Archivo WAV','2719744 bytes de audio'),box(385,120,275,130,'Buffer de canción','16384 bytes · 371,52 ms'),box(745,120,260,130,'Mitad DMA','2048 bytes · 46,44 ms'),arrow(305,185,375,185),arrow(665,185,735,185),
    txt(342,100,'leer',18,anchor='middle'),txt(700,100,'mezclar',18,anchor='middle'),rect(50,330,940,100,'#f7e3db'),txt(75,370,'Los 371,52 ms de canción no pasan solos al dispositivo.',25,CORAL,weight='bold'),txt(75,410,'El bucle debe seguir atendiendo el mezclador dentro del plazo DMA.',22)
], 'Tener datos leídos por adelantado no autoriza a bloquear el bucle hasta agotar ese buffer.', 'ch16.md')
save('capas','No confundir el socket IPX con el puerto UDP',[
    box(50,100,290,110,'Juego A','socket IPX 0x869C'),box(700,100,290,110,'Juego B','socket IPX 0x869C'),box(50,300,290,110,'DOSBox A','transporte UDP'),box(700,300,290,110,'DOSBox B','puerto 5213'),arrow(195,220,195,285),arrow(845,285,845,220),arrow(355,355,685,355),txt(515,315,'red anfitriona',22,anchor='middle'),txt(80,465,'Dentro del datagrama: cabecera IPX de 30 bytes + envoltorio de 12 + datos.',21)
], 'El encapsulado del emulador no cambia la API IPX que utiliza el juego DOS.', 'ch17.md')
save('datagrama','Descubrir una pareja y enviar mensajes',[
    box(50,90,260,90,'Equipo A'),box(730,90,260,90,'Equipo B'),arrow(315,220,720,220),txt(515,200,'HELLO por broadcast',22,anchor='middle'),arrow(725,300,320,300),txt(515,280,'HELLO_ACK',22,anchor='middle'),arrow(315,380,720,380),txt(515,360,'Datos para el peer identificado',22,anchor='middle'),txt(65,465,'Envío lanzado no significa entrega confirmada: el protocolo superior decide.',21)
], 'El descubrimiento establece una identidad y una dirección; los datagramas siguen necesitando reglas de uso.', 'ch18.md')
e=[txt(35,102,'Retardo de entrada: 5 frames de simulación',25,weight='bold')]
for i in range(9):
    x=50+i*107
    e += [rect(x,145,94,75,TEAL if i>=5 else '#dce7e8'),txt(x+47,190,'N='+str(i),21,WHITE if i>=5 else INK,anchor='middle'),txt(x+47,260,'entrada' if i>=5 else '0',18,anchor='middle')]
e += [arrow(98,330,635,330),txt(350,310,'La tecla recogida en N=0 se aplica en N=5',22,anchor='middle'),txt(70,405,'Cada paso necesita las entradas de ambos jugadores para ese mismo N.',22),txt(70,450,'Las cámaras pueden diferir; el estado compartido por frame debe coincidir.',22)]
save('lockstep','Enviar entradas futuras, simular entradas disponibles',e,'El retardo inicial permite que los mensajes lleguen antes del frame donde se necesitan.', 'ch19.md')
save('checksum','Comparar el mismo frame, aunque llegue más tarde',[
    box(40,110,270,140,'Estado local N=412','checksum 51230\ncasilla 412 & 63 = 28'),box(730,110,270,140,'Mensaje remoto','frame 412\nchecksum 51237'),arrow(725,180,325,180),
    box(355,325,330,100,'Comparación válida','las etiquetas de frame coinciden'),arrow(180,260,400,315),arrow(865,260,640,315),txt(70,465,'Una diferencia detecta divergencia en el frame comparado; no localiza su origen.',21)
], 'La etiqueta evita comparar un checksum nuevo con otro antiguo que ocupa la misma casilla del anillo.', 'ch20.md')
# Camera and map sequence.
e=[rect(30,80,640,400,'#dceae5',NAVY),txt(45,110,'MUNDO · 640 × 400',21,NAVY,weight='bold')]
for x in range(0,641,40): e.append(line(30+x,80,30+x,480,LINE,1))
for y in range(0,401,40): e.append(line(30,80+y,670,80+y,LINE,1))
e += [rect(150,160,320,200,'#c9eae3',TEAL,0),tank(200,190,1.4),txt(158,190,'cámara (120,80)',18),txt(225,230,'tanque (170,110)',18),rect(730,140,280,175,'#c9eae3',TEAL,0),tank(730+50*.875,140+30*.875,1.4),txt(740,340,'Pantalla: (50,30)',23,TEAL,weight='bold'),arrow(485,250,715,230),txt(725,405,'170 − 120 = 50',21),txt(725,445,'110 − 80 = 30',21)]
save('mundo','La cámara es la esquina de una ventana',e,'Los objetos conservan sus coordenadas de mundo. La resta de cámara solo calcula dónde dibujarlos.', 'ch21.md',height=520)
e=[]
letters='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdef'
for y in range(4):
    for x in range(8):
        active=1<=y<=2 and 2<=x<=5
        e += [rect(30+x*61,100+y*66,56,55,TEAL if active else '#dce7eb',radius=3),txt(58+x*61,135+y*66,letters[y*8+x],23,WHITE if active else INK,anchor='middle')]
for y,row in enumerate(['KLMN','STUV']):
    for x,ch in enumerate(row):
        e += [rect(690+x*76,145+y*90,66,70,TEAL,radius=4),txt(723+x*76,190+y*90,ch,27,WHITE,anchor='middle')]
e += [arrow(535,230,670,230),txt(40,415,'Origen: fila 1 → byte 10; fila 2 → byte 18.',23),txt(40,455,'Avance de origen: 8. Avance de destino: 4.',24,TEAL,weight='bold')]
save('stride','Una ventana no es una copia continua',e,'La siguiente fila visible empieza después del stride del mapa, no después de la anchura copiada.', 'ch22.md')
e=[txt(45,110,'Una misma dirección física admite varias parejas:',24),box(55,145,340,110,'1000:0020','0x1000 × 16 + 0x20'),box(645,145,340,110,'1002:0000','0x1002 × 16 + 0x00'),txt(520,212,'=',42,TEAL,anchor='middle'),txt(520,325,'0x10020',36,TEAL,anchor='middle',weight='bold'),txt(55,405,'El puntero huge normaliza. El offset de mapa se calcula en 32 bits.',23),txt(55,455,'Reserva grande + producto amplio + puntero adecuado: necesitas los tres.',22)]
save('huge','Memoria segmentada: dirección y tamaño son problemas distintos',e,'Ampliar un resultado después de una multiplicación de 16 bits desbordada no la repara.', 'ch23.md')
e=[rect(100,110,360,300,'#dce8e9',NAVY,0),rect(28,78,144,144,'#f5d2c8',CORAL,0),rect(100,110,72,112,TEAL,radius=0),txt(220,150,'PANTALLA',23),txt(25,65,'sprite (−9,−4)',21,CORAL),txt(520,140,'Sprite original: 18 × 18',24,weight='bold'),txt(520,205,'Columnas visibles: 9..17',23),txt(520,260,'Filas visibles: 4..17',23),txt(520,320,'Resultado: 9 × 14',29,TEAL,weight='bold'),txt(60,465,'Se recorta el dibujo. No se cambia la posición del objeto en el mundo.',22)]
save('clipping','Un origen negativo todavía puede dejar píxeles visibles',e,'La intersección conserva 126 píxeles geométricos, antes de descartar los transparentes.', 'ch24.md')
e=[rect(30,80,640,400,'#e3eeeb',NAVY,0),rect(230,220,240,120,'#c6e8dc',TEAL,0),tank(434,254,2),line(230,80,230,480,TEAL,2,'6 5'),line(470,80,470,480,TEAL,2,'6 5'),txt(80,135,'100 px',23),txt(520,135,'100 px',23),txt(235,210,'zona interior',21,TEAL),txt(725,140,'Pantalla 320 × 200',24,weight='bold'),txt(725,205,'Tanque 18 × 18',23),txt(725,275,'Esquina permitida:',23),txt(725,320,'X: 100..202',27,TEAL,weight='bold'),txt(725,365,'Y: 70..112',27,TEAL,weight='bold'),txt(725,435,'Al sobresalir, empuja',19),txt(725,465,'la cámara lo justo.',19)]
save('zona','Zona muerta: el tanque entero debe caber',e,'El borde físico derecho está en 220, pero la esquina del tanque solo puede llegar a 202.', 'ch25.md',height=520)
e=[txt(45,110,'Avanzar, frenar y volver',24,weight='bold')]
tx=[300,302,304,306,304,204,202]; cx=[100,100,102,104,104,104,102]
for i,(a,b) in enumerate(zip(tx,cx)):
    x=45+i*140
    e += [rect(x,140,127,215,PALE,LINE),txt(x+63,178,i+1,20,anchor='middle'),txt(x+63,230,a,27,anchor='middle'),txt(x+63,290,b,27,TEAL,anchor='middle')]
e += [txt(50,397,'Fila superior: X del tanque en mundo. Fila verde: X de cámara.',21),txt(50,448,'Al volver, el tanque atraviesa la zona antes de arrastrar la cámara.',23,TEAL)]
save('traza','La cámara conserva historia',e,'Traza numérica de la lección 25: la cámara no retrocede inmediatamente al invertir la marcha.', 'ch25.md')
e=[]
for i,(label,cx,cy) in enumerate([('superior izquierda',0,0),('superior derecha',320,0),('inferior izquierda',0,200),('inferior derecha',320,200)]):
    x=35+(i%2)*510; y=90+(i//2)*190
    e += [rect(x,y,220,140,'#dce8e7',NAVY,0),rect(x+cx/640*220,y+cy/400*140,110,70,'#96cfc0',TEAL,0),txt(x+240,y+40,label,19),txt(x+240,y+85,f'({cx},{cy})',26,TEAL,weight='bold')]
e += [txt(50,490,'La esquina de cámara llega a (320,200), no al último píxel del mundo.',22)]
save('limites','Cuatro extremos válidos de la ventana',e,'Restar el tamaño de pantalla al tamaño de mapa deja espacio para la ventana completa.', 'ch25.md',height=530)
e=[txt(35,100,'Paredes en X = 0, 3 y 7 de una fila de ocho píxeles',24)]
for x in range(8):
    on=x in (0,3,7)
    e += [rect(45+x*120,140,105,78,TEAL if on else '#dce7eb'),txt(97+x*120,190,int(on),29,WHITE if on else INK,anchor='middle'),txt(97+x*120,260,'bit '+str(x),19,anchor='middle')]
e += [txt(70,345,'1 + 8 + 128 = 137 = 0x89',31,TEAL,weight='bold'),txt(70,415,'i = y × anchura + x    byte = i / 8    máscara = 1 << (i % 8)',23),txt(70,460,'Al escribir: OR. Al consultar: AND distinto de cero.',23)]
save('mascara','Ocho preguntas de pared en un solo byte',e,'El píxel inicial usa el bit menos significativo; la escritura binaria habitual se muestra en orden inverso.', 'ch26.md')
e=[]
for n,(title,col) in enumerate([('SKYNET',TEAL),('MILITAR','#7b854f'),('NEON','#8463b7')]):
    x=40+n*335
    e += [rect(x,110,295,235,'#dce8e4',NAVY,0),rect(x+15,125,265,18,col),rect(x+15,312,265,18,col),rect(x+15,125,18,205,col),rect(x+262,125,18,205,col),rect(x+105,145,22,105,col),rect(x+190,225,22,95,col),tank(x+60,260,2,col),txt(x+147,385,title,25,col,anchor='middle',weight='bold')]
e += [txt(75,455,'Un nivel, tres aspectos: misma máscara lógica, distintas paletas y dibujos.',22)]
save('temas','La geometría compartida no depende del tema',e,'Esquema didáctico de tres aspectos sobre una misma distribución; no es una captura de un nivel del juego.', 'ch27.md')
e=[box(35,110,280,125,'Separación de salidas','dx=317 · dy=193'),box(380,110,280,125,'Distancia aproximada','317 + 193/2 = 413'),box(725,110,280,125,'Normalización','máximo geométrico 813'),arrow(320,172,370,172),arrow(665,172,715,172),txt(520,325,'100 − floor(413 × 100 / 813) = 50',30,TEAL,anchor='middle',weight='bold')]
for i,ch in enumerate('050%'):
    e += [rect(348+i*87,360,82,77,NAVY),txt(389+i*87,414,ch,44,WHITE,anchor='middle',weight='bold')]
e += [txt(520,478,'El HUD usa pantalla. Mover solo la cámara no cambia el porcentaje.',21,anchor='middle')]
save('radar','Cercanía: una cifra, no una ruta',e,'La distancia ignora las paredes; indica separación geométrica aproximada.', 'ch28.md',height=510)
save('integracion','La música comienza después de las cargas',[
    box(35,110,280,130,'Cargar mundo','mapas + sprites + WAV'),box(380,110,280,130,'Barrera de red','ambos extremos participan'),box(725,110,280,130,'Comenzar canción','primer frame + fundido'),arrow(320,175,370,175),arrow(665,175,715,175),
    rect(55,315,930,115,'#dcefeb'),txt(80,358,'La barrera está fuera de la condición «hay tarjeta de sonido».',23,TEAL,weight='bold'),txt(80,399,'Un equipo silencioso sigue formando parte del protocolo compartido.',22)
], 'La barrera reduce diferencias de arranque debidas a carga, pero no constituye un reloj de audio compartido.', 'ch29.md')
save('familias','Tres familias de trucos gráficos',[
    box(35,120,300,150,'Cambiar interpretación','cycle\nlos índices no se mueven'),box(370,120,300,150,'Mover tramos','scroll · wobble · ripple\nfilas divididas en copias'),box(705,120,300,150,'Buscar muestras','mosaic · zoom · rotozoom\nun origen por región o píxel'),
    txt(80,360,'Todas comparten tiempo, teclado, sonido, memoria y presentación.',24),txt(80,425,'El efecto decide la transformación; el reproductor organiza la secuencia.',22)
], 'Elegir el trabajo por paleta, tramo o píxel permite entender el coste de cada efecto.', 'ch30.md')
save('entregas','Del primer píxel a una partida completa',[
    *[box(40+(i%4)*250,105+(i//4)*170,225,120,t,s) for i,(t,s) in enumerate([('1. Imagen','VGA + BMP'),('2. Tanque','entrada + estado'),('3. Duelo','paredes + disparos'),('4. Audio','mezcla + streaming'),('5. Red','entradas por frame'),('6. Mundo','mapa + cámara'),('7. Presentación','radar + fundidos'),('8. Intro','efectos + limpieza')])],
    txt(60,460,'Cada entrega necesita un resultado observable y un caso de prueba reproducible.',21)
], 'La reconstrucción se organiza por comportamientos verificables, no por cantidad de archivos copiados.', 'ch31.md')

# Effect figures: visual examples made from code-native shapes, not modified assets.
e=[]
colors=[CORAL,TEAL,GOLD,'#8062b4']
for row in range(2):
    e.append(txt(35,155+row*150,'t=0' if row==0 else 't=1',24))
    for i in range(4):
        e += [rect(145+i*215,95+row*150,190,110,colors[(i+row)%4]),txt(240+i*215,160+row*150,'index '+str(i+1),24,WHITE,anchor='middle')]
e.append(txt(65,450,'Mismas posiciones e índices; distinta tabla de colores.',25,TEAL,weight='bold'))
save('fx_cycle','Cycle · la imagen quieta, la paleta en movimiento',e,'Una permutación cambia el aspecto de todos los píxeles con cada índice.', 'demos/cycle/doc/README.md')
e=[]
for y,row in enumerate(['ABCDEFGH','FGHABCDE']):
    for x,ch in enumerate(row):
        col=TEAL if ch in 'FGH' else NAVY
        e += [rect(50+x*120,120+y*160,108,94,col),txt(104+x*120,180+y*160,ch,33,WHITE,anchor='middle')]
e += [txt(60,95,'Origen',23),txt(60,255,'Destino: +3 a la derecha',23),txt(65,450,'Dos segmentos intercambian orden sin perder ni duplicar elementos.',23)]
save('fx_scroll','Scroll · partir la fila en dos',e,'FGH se envuelve al principio; ABCDE se copia desplazado tres posiciones.', 'demos/scroll/doc/README.md')
e=[]
for i,h in enumerate([0,80,160]):
    y=105+i*105
    e += [rect(40,y,700,72,NAVY,radius=0),rect(40+(160-h)*700/320,y,2*h*700/320,72,TEAL,radius=0),txt(775,y+43,'half='+str(h),24)]
e += [txt(60,455,'(160−half) + 2×half + (160−half) = 320',28,TEAL,weight='bold')]
save('fx_blinds','Blinds · la anchura visible crece desde el centro',e,'El cálculo necesita ampliar a long antes de multiplicar por 160 en Turbo C.', 'demos/blinds/doc/README.md')
e=[]
for i,v in enumerate([0,1,3,4,5]):
    y=95+i*65
    e += [rect(40,y,670,48,'#e0e9ed'),rect(40+v*55,y,230,48,TEAL,radius=0),txt(740,y+31,f'band {i}: v={v}',23)]
e.append(txt(55,465,'La velocidad de la banda es fija; su posición se acumula y se envuelve.',23))
save('fx_stripes','Stripes · posiciones separadas por velocidades distintas',e,'Esquema de desplazamientos tras varios pasos, mostrando las primeras cinco bandas.', 'demos/stripes/doc/README.md')
for name,ripple in [('wobble',False),('ripple',True)]:
    e=[rect(40,90,920,340,'#e2eceb')]
    for y in range(0,200,4):
        d=abs(y-100) if ripple else y
        amp=20*(100-d)/100 if ripple else 24
        dx=math.sin(d*(4 if ripple else 3)*2*math.pi/256)*amp
        yy=95+y*1.6
        for x in [170,350,530,710]: e.append(line(x+dx*2,yy,x+dx*2,yy+6,TEAL,3))
    e += [txt(50,465,'Amplitud atenuada según |y−100|.' if ripple else 'Amplitud constante; fase distinta en cada fila.',24,TEAL,weight='bold')]
    save('fx_'+name,name.capitalize()+' · deformar líneas verticales por filas',e,'Esquema continuo para leer la forma de la onda; la implementación usa la tabla entera.',f'demos/{name}/doc/README.md')
e=[rect(80,100,640,300,NAVY,radius=0),rect(140,130,580,270,TEAL,radius=0),txt(445,265,'imagen trasladada',28,WHITE,anchor='middle'),txt(780,175,'+30 X',25,TEAL),txt(780,225,'+20 Y',25,TEAL),txt(80,455,'Se borra el fondo y se copia la intersección. No hay wrap.',25)]
save('fx_bounce','Bounce · el borde deja espacio negro',e,'Ejemplo proporcional de una imagen desplazada hacia derecha y abajo, con recorte.', 'demos/bounce/doc/README.md')
e=[]
for x0,block in [(45,1),(385,4),(725,8)]:
    for y in range(0,16,block):
        for x in range(0,16,block):
            col=[TEAL,NAVY,GOLD,CORAL][((x//3)+(y//4))%4]
            e.append(rect(x0+x*17,120+y*17,block*17,block*17,col,radius=0))
    e.append(txt(x0+136,435,'B='+str(block),25,anchor='middle'))
save('fx_mosaic','Mosaic · una muestra por bloque',e,'Patrón didáctico de 16 × 16: cada bloque conserva la muestra de su esquina superior izquierda.', 'demos/mosaic/doc/README.md')
e=[]
for i,(step,visible) in enumerate([(128,'2×'),(256,'1×'),(512,'0,5×')]):
    x=35+i*340
    e += [rect(x,120,300,190,NAVY,radius=0),rect(x+(75 if step==512 else 0),120+(47.5 if step==512 else 0),150 if step==512 else 300,95 if step==512 else 190,TEAL,radius=0),txt(x+150,225,visible,35,WHITE,anchor='middle'),txt(x+150,365,'step='+str(step),25,anchor='middle')]
e.append(txt(60,455,'Paso de lectura mayor → imagen visible más pequeña.',28,TEAL,weight='bold'))
save('fx_zoom','Zoom · el paso es inverso a la ampliación',e,'El paso se expresa con ocho bits fraccionarios: 256 significa una muestra por píxel.', 'demos/zoom/doc/README.md')
e=[rect(80,100,490,300,NAVY,radius=0),f'<polygon points="205,100 565,240 445,400 85,260" fill="{TEAL}"/>',line(80,250,570,250,WHITE,3),txt(100,445,'Fila: negro | muestras válidas | negro',22),txt(630,145,'U permite: 2..8',24),txt(630,210,'V permite: 5..11',24),txt(630,280,'Intersección: 5..8',28,TEAL,weight='bold'),txt(630,350,'first = max(inicios)',22),txt(630,405,'last = min(finales)',22)]
save('fx_rotozoom','Rotozoom · intersectar dos intervalos',e,'El tramo visible necesita cumplir simultáneamente los límites de U y de V.', 'demos/rotozoom/doc/README.md')

# English workshop diagrams use the same geometry with translated labels.
translations = {
 'cycle': ('Cycle · still indices, moving palette', 'A permutation changes the colour assigned to each index.', {'Mismas posiciones e índices; distinta tabla de colores.':'Same positions and indices; a different colour table.'}),
 'scroll': ('Scroll · split a row into two spans', 'FGH wraps to the beginning while ABCDE moves three positions right.', {'Origen':'Source','Destino: +3 a la derecha':'Destination: +3 to the right','Dos segmentos intercambian orden sin perder ni duplicar elementos.':'Two spans change order without losing or duplicating elements.'}),
 'blinds': ('Blinds · an opening grows from the centre', 'Widen the product to long before multiplying by 160 on Turbo C.', {}),
 'stripes': ('Stripes · positions accumulate different velocities', 'The first five bands, shown schematically after several updates.', {'La velocidad de la banda es fija; su posición se acumula y se envuelve.':'Band velocity is fixed; position accumulates and wraps.'}),
 'wobble': ('Wobble · deform vertical lines one row at a time', 'A continuous illustration of the shape; the source samples an integer table.', {'Amplitud constante; fase distinta en cada fila.':'Constant amplitude; a different phase for each row.'}),
 'ripple': ('Ripple · reduce amplitude towards the edges', 'A continuous illustration; rows at equal distance share displacement.', {'Amplitud atenuada según |y−100|.':'Amplitude attenuated according to |y−100|.'}),
 'bounce': ('Bounce · translation leaves black borders', 'A schematic image moves right and down and is clipped to the viewport.', {'imagen trasladada':'translated image','Se borra el fondo y se copia la intersección. No hay wrap.':'Clear the background and copy the intersection. No wrapping.'}),
 'mosaic': ('Mosaic · one sample per block', 'A 16 × 16 teaching pattern sampled at each block’s top-left corner.', {}),
 'zoom': ('Zoom · sampling step is inverse to magnification', 'The sampling step has eight fractional bits; 256 represents one.', {'0,5×':'0.5×','Paso de lectura mayor → imagen visible más pequeña.':'Larger sampling step → smaller visible image.'}),
 'rotozoom': ('Rotozoom · intersect two valid intervals', 'A source pixel must satisfy both the U and V bounds.', {'Fila: negro | muestras válidas | negro':'Row: black | valid samples | black','U permite: 2..8':'U allows: 2..8','V permite: 5..11':'V allows: 5..11','Intersección: 5..8':'Intersection: 5..8','first = max(inicios)':'first = max(starts)','last = min(finales)':'last = min(ends)'}),
}
for effect,(title,caption,labels) in translations.items():
    original=REGISTRY['fx_'+effect]
    svg=(OUT/('fx_'+effect+'.svg')).read_text()
    labels={original['title']:title,original['caption']:caption,**labels}
    for old,new in labels.items(): svg=svg.replace(escape(old),escape(new))
    name='fx_'+effect+'_en'
    (OUT/(name+'.svg')).write_text(svg)
    REGISTRY[name]={'file':'figuras/'+name+'.svg','title':title,'caption':caption,'chapter':f'demos/{effect}/doc/README-EN.md'}

# Small examples introduce camera and checksum before the game-specific code.
e=[txt(40,82,'Mundo: veinte casillas; la marca permanece en 9',22)]
for i,ch in enumerate('ABCDEFGHIJKLMNOPQRST'):
    label=ch
    fill=PALE
    if i==9:
        label='@'
        fill='#f4d4cb'
    e += [rect(45+i*48,105,44,45,fill,radius=3),txt(67+i*48,135,label,23,anchor='middle'),txt(67+i*48,174,i,17,anchor='middle')]
for camera,y in [(4,230),(6,365)]:
    e += [txt(45,y+30,'Cámara = '+str(camera),23,weight='bold'),txt(45,y+66,'Pantalla = '+str(9-camera),21,TEAL)]
    for screen_x in range(10):
        world_x=camera+screen_x
        label='ABCDEFGHIJKLMNOPQRST'[world_x]
        fill=PALE
        if world_x==9:
            label='@'
            fill='#f4d4cb'
        e += [rect(320+screen_x*65,y,60,48,fill,radius=3),txt(350+screen_x*65,y+32,label,25,anchor='middle'),txt(350+screen_x*65,y+75,screen_x,18,anchor='middle')]
save('camara_tira','Mover la ventana cambia la dirección visible',e,
     'La marca sigue en mundo 9. Las ventanas de 4 y 6 la muestran en pantalla 5 y 3.', 'ch25-paso-a-paso.md')

e=[txt(80,112,'Margen: 3',22),txt(505,112,'Zona libre: 3..6',25,TEAL,anchor='middle',weight='bold'),txt(965,112,'Margen: 3',22,anchor='end')]
for i in range(10):
    fill='#e3e9eb'
    if 3<=i<=6:fill='#c9e9df'
    e += [rect(80+i*87,145,81,80,fill,radius=4),txt(120+i*87,194,i,29,anchor='middle')]
e += [box(50,300,360,100,'Antes: mundo 11, cámara 4','Pantalla 7: sobra una casilla'),arrow(420,350,600,350),box(615,300,375,100,'Después: cámara 5','Mundo 11 → pantalla 6'),txt(80,460,'Solo cambia la cámara. El objetivo conserva su coordenada de mundo.',23)]
save('camara_zona_simple','Una casilla de exceso, una casilla de corrección',e,
     'Con pantalla de diez y márgenes de tres, la marca de una casilla puede ocupar 3 a 6.', 'ch25-paso-a-paso.md')

e=[box(30,115,300,145,'A: (4,7)','4 × 3 + 7 × 5\n12 + 35 = 47'),box(370,115,300,145,'B: (5,7)','5 × 3 + 7 × 5\n15 + 35 = 50'),box(710,115,300,145,'C: (9,4)','9 × 3 + 4 × 5\n27 + 20 = 47'),txt(45,335,'A frente a B: 47 ≠ 50. La comprobación encuentra una diferencia.',23),txt(45,390,'A frente a C: 47 = 47. Los datos distintos producen una colisión.',23),txt(45,452,'Mismos pesos y mismo recuento en ambos cuadernos.',25,TEAL,weight='bold')]
save('checksum_cuadernos','Un resumen pequeño puede detectar diferencias',e,
     'La receta multiplica manzanas por tres y peras por cinco; dos inventarios distintos pueden coincidir.', 'ch20-paso-a-paso.md')

e=[box(30,115,285,150,'A termina frame 29','Calcula 25188\nGuarda (29,25188)'),arrow(325,190,365,190),box(375,115,285,150,'A comienza frame 30','Envía el pendiente\n(29,25188)'),arrow(670,190,710,190),box(720,115,290,150,'B recibe más tarde','Busca su frame 29\nCompara los valores'),txt(45,340,'La etiqueta sigue siendo 29 aunque el paquete viaje después.',25,TEAL,weight='bold'),txt(45,405,'Casilla = frame & 63: tanto 29 como 93 usan la casilla 29.',23),txt(45,453,'Si la etiqueta guardada ya es 93, no se compara con el recibido de 29.',22)]
save('checksum_recorrido','Calcular ahora, comparar el mismo frame después',e,
     'El resultado se etiqueta con el frame calculado; la casilla y su etiqueta protegen la comparación tardía.', 'ch20-paso-a-paso.md')

(OUT/'indice.json').write_text(json.dumps(REGISTRY,ensure_ascii=False,indent=2)+'\n')
print(f'Created {len(REGISTRY)} vector figures in {OUT}')
