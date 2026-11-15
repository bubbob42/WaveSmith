/* EditAhi.rexx -- the autotest EditAhi's driver: WaveSmith playing
   through AHI (no card, no simulated player; ENV:WaveSmith.prefs: PLAY=AHI,
   Paula's 8-bit stereo at 22050 Hz), worked by InputPoke at the places its
   trace (DEV:Test/ahi.trace) says:
     A  the settings read: AHI, the mode, the rate
     B  Listen at In: through AHI; the play head moves; it ends by itself
        some 3 s later (its own end: the clock, or AHI's hook)
     C  Play / Loop? on, Listen at In: played again and again; Stop
     D  Settings / Play through / Automatic (the sub-item): Listen at In
        through AHI all the same -- the test disk has no prisma.library
     E  Quit */

options results
signal on syntax
call open(log, 'DEV:Test/auto.log', 'A')
if ~show('L', 'rexxsupport.library') then call addlib('rexxsupport.library', 0, -30, 0)

box. = ''
trim. = ''
one. = ''
last. = ''

call writeln(log, 'A: the settings')
do i = 1 to 120 while trim.PLAN = ''
	call delay(50)
	call readtrace
end
if trim.PLAN = '' then do
	call writeln(log, 'FAILED no plan after 60 s')
	exit 20
end
parse var last.PLAYWAY . . . pway pid pfreq
if pway ~= 2 | pid ~= 131081 | pfreq ~= 22050 then call writeln(log, 'FAILED the way:' last.PLAYWAY', not AHI 131081 22050')
else call writeln(log, 'ok   Play through AHI, Paula:8 bit stereo at 22050 Hz')

call writeln(log, 'B: Listen at In (the Trim window, Right-Amiga T)')
w0 = n.TRIMWIN
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=20 QUAL=128'
do i = 1 to 20 while n.TRIMWIN = w0
	call delay(10)
	call readtrace
end
if box.triminlisten = '' then call writeln(log, 'FAILED Right-Amiga T: no Trim window (its Listen not in the trace)')
p0 = n.PLAY
o0 = n.PLAYEDOUT
b0 = n.BEATS
call time('R')
call clicktrim box.triminlisten
do i = 1 to 40 while n.PLAY = p0
	call delay(5)
	call readtrace
end
parse var last.PLAYTHROUGH . through
if n.PLAY ~= p0 + 1 then call writeln(log, 'FAILED Listen at In: no play line (status "'last.STATUS'")')
else if through ~= 1 then call writeln(log, 'FAILED Listen at In: through' through', not AHI (1)')
else call writeln(log, 'ok   Listen at In: playing through AHI,' last.PLAY)
do i = 1 to 100 while n.PLAYEDOUT = o0
	call delay(10)
	call readtrace
end
took = time('E')
if n.PLAYEDOUT = o0 then call writeln(log, 'FAILED Listen at In: not played out after 10 s')
else if took < 2.5 | took > 6 then call writeln(log, 'FAILED Listen at In: played out after' took 's, not about 3')
else call writeln(log, 'ok   played out by itself after' took 's')
parse var last.BEATS . . bn .
/* "beats count ms N N": the head's beats while it played */
if n.BEATS = b0 | bn < 10 then call writeln(log, 'FAILED the play head: beats "'last.BEATS'"')
else call writeln(log, 'ok   the play head moved:' last.BEATS)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-ahi.ppm'

call writeln(log, 'C: Loop')
parse var last.LOOPITEM . . . tx ix iy
call pick tx ix iy
call readtrace
l0 = n.LOOPED
call clicktrim box.triminlisten
do i = 1 to 120 while n.LOOPED < l0 + 2
	call delay(10)
	call readtrace
end
if n.LOOPED < l0 + 2 then call writeln(log, 'FAILED Loop: looped' n.LOOPED - l0 'times in 12 s')
else call writeln(log, 'ok   Loop: played again' n.LOOPED - l0 'times')
call deckkey 3
call delay(50)
call pick tx ix iy
call readtrace
if word(last.LOOP, 2) ~= 0 then call writeln(log, 'FAILED Loop? off: "'last.LOOP'"')

call writeln(log, 'D: Settings / Play through / Automatic')
parse var last.WAYAUTOITEM . . . . tx ix iy sx
sy = one.WAYAUTOY
w0 = n.PLAYWAY
address command 'DEV:Test/InputPoke X='tx' Y=4 MENU=1'
call delay(15)
address command 'DEV:Test/InputPoke X='ix' Y='iy' MENU=2'
call delay(25)
address command 'DEV:Test/InputPoke X='sx' Y='sy' MENU=2 STEPS=4'
call delay(15)
address command 'DEV:Test/InputPoke MENU=0'
call delay(50)
call readtrace
parse var last.PLAYWAY . . . pway .
if n.PLAYWAY ~= w0 + 1 | pway ~= 0 then call writeln(log, 'FAILED Automatic: the way' last.PLAYWAY)
else call writeln(log, 'ok   Play through: Automatic')
p0 = n.PLAY
call clicktrim box.triminlisten
do i = 1 to 40 while n.PLAY = p0
	call delay(5)
	call readtrace
end
parse var last.PLAYTHROUGH . through
if n.PLAY ~= p0 + 1 | through ~= 1 then call writeln(log, 'FAILED Automatic without the card: through' through)
else call writeln(log, 'ok   Automatic without the card: through AHI')
call deckkey 3
call delay(50)

call writeln(log, 'E: Quit')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=16 QUAL=128'
call delay(150)
address command 'Status >NIL: COMMAND=DEV:sasc/WaveSmith'
if rc = 0 then call writeln(log, 'FAILED Quit: still running')
else call writeln(log, 'ok   Quit')
exit 0

syntax:
	call writeln(log, 'FAILED ARexx error' rc '(' || errortext(rc) || ') in line' sigl)
	exit 20

pick: procedure
	parse arg tx ix iy
	address command 'DEV:Test/InputPoke X='tx' Y=4 MENU=1'
	call delay(15)
	address command 'DEV:Test/InputPoke X='ix' Y='iy' MENU=2'
	call delay(15)
	address command 'DEV:Test/InputPoke MENU=0'
	call delay(25)
	return

click: procedure
	parse arg l t w h
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='l + w % 2' Y='t + h % 2' STEPS=4'
	return

/* a key of the deck -- rewind 0, play 1, forward 2, stop 3, pause 4 -- a
   fifth of its box each (tapedeck.gadget's five keys side by side) */
deckkey: procedure expose box.
	parse arg k
	parse var box.DECK l t w h
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='l + (w * (2 * k + 1)) % 10' Y='t + h % 2' STEPS=4'
	return

/* a gadget of the Trim window (its title: "Trim") */
clicktrim: procedure
	parse arg l t w h
	address command 'DEV:Test/InputPoke WIN=Trim X='l + w % 2' Y='t + h % 2' STEPS=4'
	return

readtrace: procedure expose box. trim. one. last. n.
	n. = 0
	if ~open(tr, 'DEV:Test/ahi.trace', 'R') then return
	do while ~eof(tr)
		line = readln(tr)
		parse var line what rest
		what = upper(what)
		n.what = n.what + 1
		last.what = rest
		if words(rest) = 1 then one.what = strip(rest)
		else if words(rest) = 8 then do
			parse var rest . . . . a b c d
			trim.what = a b c d
		end
		else if words(rest) = 4 & datatype(word(rest, 1), 'W') then box.what = rest
	end
	call close(tr)
	return
