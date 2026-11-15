/* EditGain.rexx -- the autotest EditGain's driver: WaveSmith's Effects
   menu, Gain..., Normalise and Remove fades and gain (concept 3.2),
   worked by InputPoke at the places its trace (DEV:Test/gain.trace)
   says; the settings PLAY=AHI and NORMALISE=20 (-2.0 dB):
     A  the suggestion there; a part marked (columns 100 .. 200)
     B  Right-Amiga G: the Gain window; 4.5 typed in its field and
        Return: the waveform behind at +4.5 dB; Use: three pieces, changed
     C  another part (columns 400 .. 480), Effects/Gain... from the menu:
        Normalise (to the setting's -2.0 dB) shown behind; Listen: the
        part played through AHI while the window is open, the play head
        moving; Cancel: the pieces as they were, three
     D  Effects/Normalise from the menu: the gain the window's Normalise
        showed, at once; five pieces
     E  columns 90 .. 210 marked, Effects/Remove fades and gain: the +4.5
        dB gone, three pieces
     F  nothing marked (a click), Remove fades and gain: In .. Out (F5),
        one piece; Undo: three again
     G  Save (Right-Amiga S): saved, nothing unsaved
     H  Quit (Right-Amiga Q): nothing asked
   German keymap: G raw 36, S 33, Z 21, Q 16; digits 1-9 raw 1-9, '.' 57,
   Del 70, Backspace 65, Return 68. */

options results
signal on syntax
call open(log, 'DEV:Test/auto.log', 'A')
if ~show('L', 'rexxsupport.library') then call addlib('rexxsupport.library', 0, -30, 0)

box. = ''
trim. = ''
one. = ''
last. = ''
selected = ''

call writeln(log, 'A: the suggestion, a part marked')
do i = 1 to 120 while trim.PLAN = ''
	call delay(50)
	call readtrace
end
if trim.PLAN = '' then do
	call writeln(log, 'FAILED no plan after 60 s')
	exit 20
end
parse var trim.PLAN pin pout .
parse var box.wave wl wt ww wh
ky = wt + wh % 2
fc = one.FIRSTCOL
call mark wl + fc + 100, wl + fc + 200, ky
call readtrace
parse var selected sa sb
if sa = '' | sb <= sa then call writeln(log, 'FAILED no part marked ("'last.SELECTED'")')
else call writeln(log, 'ok   marked' sa'..'sb)

call writeln(log, 'B: Right-Amiga G, the Gain window: 4.5 dB, Use')
w0 = n.GAINWIN
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=36 QUAL=128'
call gainwait w0
if n.GAINWIN = w0 then do
	call writeln(log, 'FAILED Right-Amiga G: no Gain window (status "'last.STATUS'")')
	call quit
	exit 20
end
parse var trim.GAIN gf ge gp
call writeln(log, 'ok   the Gain window at' box.GAINWIN', the part' gf'..'ge', its peak' gp)
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-gain-window.ppm'
call gaintype '4 57 5'
call readtrace
parse var last.GAINPREVIEW . . pl pt
parse var last.TIMELINE . . tp tf
if pt ~= 1045 then call writeln(log, 'FAILED 4.5 typed: the preview "'last.GAINPREVIEW'", not tenths 1045')
else if tp ~= 3 then call writeln(log, 'FAILED 4.5 typed: the waveform behind with' tp 'pieces, not 3')
else call writeln(log, 'ok   4.5 typed: the waveform behind at level' pl', 3 pieces')
/* the waveform behind the window painted at once (the window holds the
   loop: no idle pieces); a kept view had columns to paint */
parse var trim.GAINPAINT gb ga go
if n.GAINPAINT = 0 then call writeln(log, 'FAILED 4.5 typed: the preview not painted (no gainpaint line)')
else if gb = 0 | ga ~= 0 | go ~= 0 then call writeln(log, 'FAILED 4.5 typed: the preview painted' gb '->' ga 'columns left, the overview' go)
else call writeln(log, 'ok   4.5 typed: the preview painted while the window is open,' gb 'columns, none left')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-gain-typed.ppm'
g0 = n.GAINED
call gainclick 'GAINUSE'
call delay(100)
call readtrace
parse var trim.GAINED ua ub lv tt
parse var last.TIMELINE . . tp tf
if n.GAINED ~= g0 + 1 then call writeln(log, 'FAILED Use: no gained line ("'last.GAINKEPT'")')
else if tt ~= 1045 | lv ~= pl then call writeln(log, 'FAILED Use: level' lv 'tenths' tt', not' pl '1045')
else if abs(ua - sa) > 4000 | abs(ub - sb) > 4000 then call writeln(log, 'FAILED Use at' ua'..'ub', not the part marked' sa'..'sb)
else if tp ~= 3 then call writeln(log, 'FAILED Use:' tp 'pieces, not 3')
else if one.DIRTY ~= 1 then call writeln(log, 'FAILED Use: not marked changed')
else call writeln(log, 'ok   +4.5 dB on' ua'..'ub': 3 pieces, changed')

call writeln(log, 'C: Effects/Gain... from the menu: Normalise, Listen, Cancel')
call mark wl + fc + 400, wl + fc + 480, ky
call readtrace
parse var selected sa sb
w0 = n.GAINWIN
parse var last.GAINITEM . . . tx ix iy
call pick tx ix iy
call gainwait w0
if n.GAINWIN = w0 then do
	call writeln(log, 'FAILED Effects/Gain...: no Gain window (menu at' last.GAINITEM')')
	call quit
	exit 20
end
parse var trim.GAIN gf ge gp
call gainclick 'GAINNORMALISE'
call delay(50)
call readtrace
parse var last.GAINPREVIEW . . nl nt
parse var last.TIMELINE . . tp tf
if nt = '' | nt = 1000 then call writeln(log, 'FAILED Normalise in the window: the preview "'last.GAINPREVIEW'"')
else if tp ~= 5 then call writeln(log, 'FAILED Normalise in the window: the waveform behind with' tp 'pieces, not 5')
else call writeln(log, 'ok   Normalise in the window: tenths' nt - 1000 'from the peak' gp', 5 pieces behind')
p0 = n.PLAY
b0 = n.BEATS
call gainclick 'GAINLISTEN'
call delay(200)
call readtrace
parse var last.PLAY . . lf le
if n.PLAY ~= p0 + 1 then call writeln(log, 'FAILED Listen: nothing played ("'last.STATUS'")')
else if lf ~= gf | le ~= ge then call writeln(log, 'FAILED Listen played' lf'..'le', not the part' gf'..'ge)
else call writeln(log, 'ok   Listen: the part' lf'..'le 'playing')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-gain-listen.ppm'
call gainclick 'GAINCANCEL'
call delay(100)
call readtrace
parse var last.TIMELINE . . tp tf
parse var last.BEATS . . bn .
if one.GAINKEPT ~= 0 then call writeln(log, 'FAILED Cancel: "gainkept' last.GAINKEPT'", not used 0')
else if tp ~= 3 then call writeln(log, 'FAILED Cancel:' tp 'pieces, not 3 as before')
else call writeln(log, 'ok   Cancel: the pieces as they were, 3')
if n.BEATS = b0 | bn < 10 then call writeln(log, 'FAILED the play head in the Gain window: beats "'last.BEATS'"')
else call writeln(log, 'ok   the play head moved while the window was open:' last.BEATS)

call writeln(log, 'D: Effects/Normalise from the menu')
m0 = n.NORMALISE
parse var last.NORMALISEITEM . . . tx ix iy
call pick tx ix iy
call delay(100)
call readtrace
parse var trim.NORMALISE na nb np n2
parse var last.TIMELINE . . tp tf
if n.NORMALISE ~= m0 + 1 then call writeln(log, 'FAILED Normalise: no normalise line (menu at' last.NORMALISEITEM')')
else if n2 ~= nt | np ~= gp then call writeln(log, 'FAILED Normalise: tenths' n2 'peak' np', not the window''s' nt gp)
else if tp ~= 5 then call writeln(log, 'FAILED Normalise:' tp 'pieces, not 5')
else call writeln(log, 'ok   normalised' na'..'nb': tenths' n2 - 1000', 5 pieces')
address command 'DEV:Test/Grab 0 0 0 0 DEV:Test/edit-gain-normalised.ppm'

call writeln(log, 'E: Remove fades and gain over the +4.5 dB')
call mark wl + fc + 90, wl + fc + 210, ky
call readtrace
r0 = n.PLAIN
parse var last.PLAINITEM . . . tx ix iy
call pick tx ix iy
call delay(80)
call readtrace
parse var trim.PLAIN ra rb rc
parse var last.TIMELINE . . tp tf
if n.PLAIN ~= r0 + 1 then call writeln(log, 'FAILED Remove: no plain line (menu at' last.PLAINITEM')')
else if rc ~= 1 then call writeln(log, 'FAILED Remove: nothing changed')
else if tp ~= 3 then call writeln(log, 'FAILED Remove:' tp 'pieces, not 3')
else call writeln(log, 'ok   removed' ra'..'rb': 3 pieces')

call writeln(log, 'F: nothing marked, Remove fades and gain: In .. Out; Undo')
call mark wl + fc + 300, wl + fc + 300, ky
call readtrace
parse var selected sa sb
r0 = n.PLAIN
call pick tx ix iy
call delay(80)
call readtrace
parse var trim.PLAIN ra rb rc
parse var last.TIMELINE . . tp tf
if n.PLAIN ~= r0 + 1 then call writeln(log, 'FAILED Remove, nothing marked: no plain line (selected' sa sb')')
else if abs(ra - pin) > 4000 | abs(rb - pout) > 4000 then call writeln(log, 'FAILED Remove, nothing marked:' ra'..'rb', not In .. Out' pin'..'pout)
else if rc ~= 1 | tp ~= 1 then call writeln(log, 'FAILED Remove, nothing marked: changed' rc',' tp 'pieces, not 1 1')
else call writeln(log, 'ok   nothing marked: removed In .. Out' ra'..'rb', 1 piece')
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=21 QUAL=128'
call delay(80)
call readtrace
parse var last.TIMELINE . . up .
if up ~= 3 then call writeln(log, 'FAILED Undo of the Remove:' up 'pieces, not 3')
else call writeln(log, 'ok   Undo: 3 pieces, the normalised part back')

call writeln(log, 'G: Save')
s0 = n.SAVED
address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=33 QUAL=128'
do i = 1 to 60 while n.SAVED = s0
	call delay(25)
	call readtrace
end
/* "dirty 0" follows "saved" a few lines on: waited for */
do i = 1 to 20 while n.SAVED > s0 & one.DIRTY ~= 0
	call delay(10)
	call readtrace
end
parse var trim.SAVED si so se
if n.SAVED ~= s0 + 1 then call writeln(log, 'FAILED Save: no saved line')
else if se ~= 0 then call writeln(log, 'FAILED Save: error' se)
else if one.DIRTY ~= 0 then call writeln(log, 'FAILED Save: still marked changed')
else call writeln(log, 'ok   saved' si'..'so', nothing unsaved')

call writeln(log, 'H: Quit')
call quit
address command 'Status >NIL: COMMAND=DEV:sasc/WaveSmith'
if rc = 0 then call writeln(log, 'FAILED Quit: still running')
else call writeln(log, 'ok   Quit: nothing asked')
exit 0

syntax:
	call writeln(log, 'FAILED ARexx error' rc '(' || errortext(rc) || ') in line' sigl)
	exit 20

quit: procedure
	address command 'DEV:Test/InputPoke WIN=WaveSmith KEY=16 QUAL=128'
	call delay(150)
	return

/* the Gain window's next boxes in the trace, 5 s at most */
gainwait: procedure expose box. trim. one. last. n. selected
	parse arg w0
	do i = 1 to 20 while n.GAINWIN = w0
		call delay(25)
		call readtrace
	end
	call delay(25)
	return

/* a gadget of the Gain window clicked: its box is the window's, the
   window's the screen's */
gainclick: procedure expose box.
	parse arg what
	parse var box.GAINWIN gl gt .
	parse value box.what with l t w h
	address command 'DEV:Test/InputPoke X='gl + l + w % 2' Y='gt + t + h % 2' STEPS=4'
	return

/* the Gain window's field clicked, emptied, the raw keys typed, Return */
gaintype: procedure expose box.
	parse arg keys
	call gainclick 'GAINFIELD'
	call delay(25)
	do i = 1 to 6
		address command 'DEV:Test/InputPoke KEY=70'
		address command 'DEV:Test/InputPoke KEY=65'
	end
	do i = 1 to words(keys)
		address command 'DEV:Test/InputPoke KEY='word(keys, i)
	end
	address command 'DEV:Test/InputPoke KEY=68'
	call delay(50)
	return

mark: procedure
	parse arg x1, x2, y
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='x1' Y='y' CLICKS=0 STEPS=4'
	call delay(25)
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='x1' Y='y' SELECT=1'
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='x2' Y='y' SELECT=2 STEPS=8'
	address command 'DEV:Test/InputPoke WIN=WaveSmith X='x2' Y='y' SELECT=0 STEPS=1'
	call delay(50)
	return

pick: procedure
	parse arg tx ix iy
	address command 'DEV:Test/InputPoke X='tx' Y=4 MENU=1'
	call delay(15)
	address command 'DEV:Test/InputPoke X='ix' Y='iy' MENU=2'
	call delay(15)
	address command 'DEV:Test/InputPoke MENU=0'
	call delay(25)
	return

readtrace: procedure expose box. trim. one. last. n. selected
	n. = 0
	if ~open(tr, 'DEV:Test/gain.trace', 'R') then return
	do while ~eof(tr)
		line = readln(tr)
		parse var line what rest
		what = upper(what)
		n.what = n.what + 1
		last.what = rest
		if what = 'SELECTED' then selected = subword(rest, 3, 2)
		if words(rest) = 1 then one.what = strip(rest)
		else if what = 'GAINKEPT' then one.what = word(rest, 2)
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
