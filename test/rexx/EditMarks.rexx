/* EditMarks.rexx -- the autotest EditMarks' driver: WaveSmith's markers
   on a record side (DEV:Test/Split/Marks.wav, a copy of make_vinyl_side.py's
   side: three songs at 3.0, 40.5 and 84.3 s), read ends first
   (WaveSmithEnds 20: its middle's level not measured), worked by
   InputPoke at the places the trace (DEV:Test/marks.trace) says:
     B  Edit/Find tracks: the middle measured, two markers -- half a
        second before 40.5 s and 84.3 s (edit-mb)
     C  a click at 60 s, Return: a third marker there, between them
     D  that marker clicked: picked; Del: gone
     E  the first marker dragged 30 pixels on: moved; Undo: back
     F  Project/Split at markers: Marks-01 .. -03.wav (the Mac's check:
        make_vinyl_side.py --check dist/Test/Split Marks)
     J  F1 back to In, step by step (nothing before it), then F2: the two
        markers and Out (nothing after it)
     K  the deck's Forward from Out: the file's end; Rewind from there:
        Out, the two markers, In, then the start
     G  Save (Right-Amiga S): Marks.cue written beside it, the file read
        anew -- its markers back from the sheet, each In earlier
     H  Quit (Right-Amiga Q): nothing changed, nothing asked */

options results
signal on syntax
call open(log, 'DEV:Test/auto.log', 'A')
if ~show('L', 'rexxsupport.library') then call addlib('rexxsupport.library', 0, -30, 0)

box. = ''
trim. = ''
one. = ''
last. = ''
read = ''
rate = 44100

call writeln(log, 'A: opened, read ends first')
do i = 1 to 240 while trim.PLAN = '' | read = ''
	call delay(50)
	call readtrace
end
if trim.PLAN = '' | read = '' then do
	call writeln(log, 'FAILED no plan or no end of the reading after 120 s')
	exit 20
end
parse var trim.PLAN pin pout .
call writeln(log, 'plan (in out leadclicks tailclicks):' trim.PLAN'; ends:' last.ENDS)
if last.ENDS = '' then call writeln(log, 'FAILED the side was not read ends first (WaveSmithEnds)')
parse var box.wave wl wt ww wh
parse var trim.SHOWN . . vtop vzoom
wy = wt + (wh * 2) % 3

call writeln(log, 'B: Edit/Find tracks')
m0 = n.MARKS
parse var last.FINDTRACKSITEM . . . tx ix iy
call pick tx ix iy
do i = 1 to 120 while n.MARKS = m0
	call delay(50)
	call readtrace
end
parse var last.MARKS . . mc msel
parse var last.MARKSLIST . ma mb .
call writeln(log, 'marks:' last.MARKS '/' last.MARKSLIST)
if n.MARKS = m0 then call writeln(log, 'FAILED Find tracks: no markers after 60 s')
else if mc ~= 2 then call writeln(log, 'FAILED Find tracks:' mc 'markers, not 2')
else if abs(ma - (rate * 405 % 10 - rate % 2)) > rate * 3 % 10 | abs(mb - (rate * 843 % 10 - rate % 2)) > rate * 3 % 10 then
	call writeln(log, 'FAILED Find tracks: markers at' ma mb', not half a second before 40.5 s and 84.3 s')
else call writeln(log, 'ok   Find tracks: markers at' ma '(' || ma / rate 's) and' mb '(' || mb / rate 's)')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-mb.ppm'

call writeln(log, 'C: a click at 60 s, Return: a marker there')
x60 = wl + one.FIRSTCOL + (rate * 60 - vtop) % vzoom
call click x60 wy 0 0
call delay(50)
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=68'
call delay(50)
call readtrace
parse var last.MARKS . . mc msel
parse var last.MARKSLIST . ma mc2 mb .
if mc ~= 3 | msel ~= 1 then call writeln(log, 'FAILED Return:' mc 'markers, the one picked' msel', not 3 and 1')
else if abs(mc2 - rate * 60) > vzoom * 2 then call writeln(log, 'FAILED Return: the marker at' mc2', not at 60 s')
else call writeln(log, 'ok   Return: a marker at' mc2 '(' || mc2 / rate 's), the second of three')

call writeln(log, 'D: that marker clicked, Del')
/* the view as it opened: the whole side */
xm = wl + one.FIRSTCOL + (mc2 - vtop) % vzoom
call click xm wy 0 0
call delay(50)
call readtrace
parse var last.MARKS . . mc msel
if msel ~= 1 then call writeln(log, 'FAILED a click on the marker at column' xm': picked' msel', not 1')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=70'
call delay(50)
call readtrace
parse var last.MARKS . . mc msel
parse var last.MARKSLIST . ma mb .
if mc ~= 2 | ma = mc2 | mb = mc2 then call writeln(log, 'FAILED Del:' mc 'markers left (' || ma mb')')
else call writeln(log, 'ok   Del: the 60 s marker gone,' mc 'left')

call writeln(log, 'E: the first marker dragged 30 pixels on, Undo')
xa = wl + one.FIRSTCOL + (ma - vtop) % vzoom
address command 'DEV:Test/InputPoke WIN=WaveSmith X='xa' Y='wy' CLICKS=0 STEPS=4'
call delay(25)
address command 'DEV:Test/InputPoke WIN=WaveSmith X='xa' Y='wy' SELECT=1'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='xa + 30' Y='wy' SELECT=2 STEPS=8'
address command 'DEV:Test/InputPoke WIN=WaveSmith X='xa + 30' Y='wy' SELECT=0 STEPS=1'
call delay(50)
call readtrace
parse var last.MARKSLIST . ea .
if abs(ea - ma - 30 * vzoom) > 2 * vzoom then call writeln(log, 'FAILED the drag: the marker' ma '->' ea', not 30 columns on')
else call writeln(log, 'ok   dragged:' ma '->' ea)
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=21 QUAL=128'
call delay(50)
call readtrace
parse var last.MARKSLIST . ua .
if ua ~= ma then call writeln(log, 'FAILED Undo: the marker at' ua', not back at' ma)
else call writeln(log, 'ok   Undo: the marker back at' ua)

call writeln(log, 'F: Project/Split at markers')
s0 = n.SPLITMARKS
p0 = n.PARTNAME
parse var last.SPLITMARKSITEM . . . tx ix iy
call pick tx ix iy
do i = 1 to 240 while n.SPLITMARKS = s0
	call delay(50)
	call readtrace
end
parse var last.SPLITMARKS . . sparts serr
if n.SPLITMARKS = s0 then call writeln(log, 'FAILED Split at markers: no splitmarks line')
else if sparts ~= 3 | serr ~= 0 then call writeln(log, 'FAILED Split at markers:' sparts 'parts, error' serr)
else if ~exists('DEV:Test/Split/Marks-01.wav') | ~exists('DEV:Test/Split/Marks-02.wav') | ~exists('DEV:Test/Split/Marks-03.wav') then
	call writeln(log, 'FAILED Split at markers: Marks-01 .. -03.wav not all there')
else call writeln(log, 'ok   Split at markers: Marks-01 .. -03.wav')
/* the status line named each part as it was written, not Marks.wav */
if n.PARTNAME ~= p0 + 3 | one.PARTNAME ~= 'Marks-03.wav' then call writeln(log, 'FAILED Split at markers: the parts named' n.PARTNAME - p0 'times, the last "'one.PARTNAME'"')
else call writeln(log, 'ok   Split at markers: the status line named each part, the last Marks-03.wav')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-mf.ppm'

call writeln(log, 'J: F1 and F2: from marker to marker')
parse var last.MARKSLIST . ja jb .
/* F1 until In (a press that finds nothing writes no jump line) */
do k = 1 to 4
	j0 = n.JUMP
	address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=80'
	call delay(40)
	call readtrace
	if n.JUMP = j0 then leave
end
parse var last.JUMP . . . jn jf jto
if jn ~= 0 | jto ~= pin then call writeln(log, 'FAILED F1: the last jump "'last.JUMP'", not back to In' pin)
else if n.JUMP ~= j0 then call writeln(log, 'FAILED F1 at In: jumped on to' jto)
else call writeln(log, 'ok   F1: back to In' pin', nothing before it')
stops = ''
do k = 1 to 4
	j0 = n.JUMP
	address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=81'
	call delay(40)
	call readtrace
	if n.JUMP = j0 then leave
	parse var last.JUMP . . . jn jf jto
	stops = stops jto
end
if space(stops, 1) ~= ja jb pout then call writeln(log, 'FAILED F2: stopped at' stops', not' ja jb pout '(the markers, Out)')
else call writeln(log, 'ok   F2: the markers' ja jb', then Out' pout', nothing after it')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-mj.ppm'

call writeln(log, 'K: the deck''s Forward and Rewind: the stops, then the file''s ends')
mlen = word(read, 1)
/* from Out (F2's last stop): Forward goes on to the end */
j0 = n.JUMP
call deckkey 2
call delay(40)
call readtrace
parse var last.JUMP . . . jn jf jto
if n.JUMP ~= j0 + 1 | jto ~= mlen then call writeln(log, 'FAILED Forward past Out: "'last.JUMP'", not to the end' mlen)
else call writeln(log, 'ok   Forward past Out: the end' mlen)
/* Rewind from the end: Out, the markers, In, then the start */
stops = ''
do k = 1 to 7
	j0 = n.JUMP
	call deckkey 0
	call delay(40)
	call readtrace
	if n.JUMP = j0 then leave
	parse var last.JUMP . . . jn jf jto
	stops = stops jto
end
if space(stops, 1) ~= pout jb ja pin 0 then call writeln(log, 'FAILED Rewind: stopped at' stops', not' pout jb ja pin '0 (Out, the markers, In, the start)')
else call writeln(log, 'ok   Rewind: Out' pout', the markers' jb ja', In' pin', then the start')

call writeln(log, 'G: Save: the cue sheet, the markers back from it')
parse var last.MARKSLIST . ga gb .
c0 = n.CUE
p0 = n.PLAN
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=33 QUAL=128'
do i = 1 to 240 while n.CUE = c0 | n.PLAN = p0
	call delay(50)
	call readtrace
end
parse var last.CUE . . cn cerr
parse var last.MARKSLIST . ra rb .
if n.CUE = c0 then call writeln(log, 'FAILED Save: no cue line')
else if cn ~= 2 | cerr ~= 0 then call writeln(log, 'FAILED Save: the cue sheet' cn 'markers, error' cerr)
else if ~exists('DEV:Test/Split/Marks.cue') then call writeln(log, 'FAILED Save: no Marks.cue')
else if ra ~= ga - pin | rb ~= gb - pin then call writeln(log, 'FAILED read anew: the markers at' ra rb', not' ga - pin gb - pin '(In' pin 'earlier)')
else call writeln(log, 'ok   Save: Marks.cue, read anew: the markers at' ra rb' (In' pin 'earlier)')

call writeln(log, 'H: Quit')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=16 QUAL=128'
call delay(150)
address command 'Status >NIL: COMMAND=DEV:sasc/WaveSmith'
if rc = 0 then do
	call writeln(log, 'FAILED Quit: still running (asked?)')
	address command 'DEV:Test/InputPoke KEY=69'
end
else call writeln(log, 'ok   Quit: nothing asked')
exit 0

syntax:
	call writeln(log, 'FAILED ARexx error' rc '(' || errortext(rc) || ') in line' sigl)
	exit 20

/* a menu item picked: the title at tx on the bar, the item at ix, iy */
pick: procedure
	parse arg tx ix iy
	address command 'DEV:Test/InputPoke X='tx' Y=4 MENU=1'
	call delay(15)
	address command 'DEV:Test/InputPoke X='ix' Y='iy' MENU=2'
	call delay(15)
	address command 'DEV:Test/InputPoke MENU=0'
	call delay(25)
	return

/* a click at the middle of a box (l t w h: a point with w h 0 0) */
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

readtrace: procedure expose box. trim. one. last. n. read
	n. = 0
	if ~open(tr, 'DEV:Test/marks.trace', 'R') then return
	do while ~eof(tr)
		line = readln(tr)
		parse var line what rest
		what = upper(what)
		n.what = n.what + 1
		last.what = rest
		if what = 'READ' then parse var rest . . read
		if words(rest) = 1 then one.what = strip(rest)
		else if words(rest) = 6 then do
			parse var rest . . . a b c
			trim.what = a b c
		end
		else if words(rest) = 8 then do
			parse var rest . . . . a b c d
			trim.what = a b c d
		end
		else if words(rest) = 4 & datatype(word(rest, 1), 'W') then box.what = rest
	end
	call close(tr)
	return
