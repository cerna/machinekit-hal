# Touchy is Copyright (c) 2009  Chris Radek <chris@timeguy.com>
#
# Touchy is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# Touchy is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.



# self.mcodes = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 30, 48, 49, 50, 51,
#                52, 53, 60, 61, 62, 63, 64, 65, 66, 67, 68)
# 
# self.gcodes = (0, 10, 20, 30, 40, 50, 51, 52, 53, 70, 80, 100,
#                170, 171, 180, 181, 190, 191, 200, 210, 280, 281,
#                300, 301, 330, 331, 382, 383, 384, 385, 400, 410,
#                411, 420, 421, 430, 431, 490, 530, 540, 550, 560,
#                570, 580, 590, 591, 592, 593, 610, 611, 640, 730,
#                760, 800, 810, 820, 830, 840, 850, 860, 870, 880,
#                890, 900, 901, 910, 911, 920, 921, 922, 923, 930,
#                940, 950, 960, 970, 980, 990)

class mdi:
    def __init__(self, emc):
        self.clear()
        self.emc = emc
        self.emcstat = emc.stat()
        self.emccommand = emc.command()

        self.emcstat.poll()
        am = self.emcstat.axis_mask

        self.axes = []
        axisnames = ['X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W']
        for i in range(9):
           if am & (1<<i):
               self.axes.append(axisnames[i])

        self.gcode = 'M2'

        self.codes = {
            'M3' : ['Spindle CW', 'S'],
            'M4' : ['Spindle CCW', 'S'],
            'M6' : ['Tool change', 'T'],

            # 'A' means 'the axes'
            'G0' : ['Straight rapid', 'A'],
            'G00' : ['Straight rapid', 'A'],
            'G1' : ['Straight feed', 'A', 'F'],
            'G01' : ['Straight feed', 'A', 'F'],
            'G2' : ['Arc CW', 'A', 'I', 'J', 'K', 'R', 'F'],
            'G02' : ['Arc CW', 'A', 'I', 'J', 'K', 'R', 'F'],
            'G3' : ['Arc CCW', 'A', 'I', 'J', 'K', 'R', 'F'],
            'G03' : ['Arc CCW', 'A', 'I', 'J', 'K', 'R', 'F'],
            'G4' : ['Dwell', 'P'],
            'G04' : ['Dwell', 'P'],
            'G10' : ['Setup', 'L', 'P', 'A', 'Q', 'R'],
            'G33' : ['Spindle synchronized feed', 'A', 'K'],
            'G33.1' : ['Rigid tap', 'Z', 'K'],
            'G38.2' : ['Probe', 'A', 'F'],
            'G38.3' : ['Probe', 'A', 'F'],
            'G38.4' : ['Probe', 'A', 'F'],
            'G38.5' : ['Probe', 'A', 'F'],
            'G41' : ['Radius compensation left', 'D'],
            'G42' : ['Radius compensation right', 'D'],
            'G41.1' : ['Radius compensation left, immediate', 'D', 'L'],
            'G42.1' : ['Radius compensation right, immediate', 'D', 'L'],
            'G43' : ['Tool length offset', 'H'],
            'G43.1' : ['Tool length offset immediate', 'I', 'K'],
            'G53' : ['Motion in unoffset coordinates', 'G', 'A', 'F'],
            'G64' : ['Continuous mode', 'P'],
            'G76' : ['Thread', 'Z', 'P', 'I', 'J', 'K', 'R', 'Q', 'H', 'E', 'L'],
            'G81' : ['Drill', 'A', 'R', 'L', 'F'],
            'G82' : ['Drill with dwell', 'A', 'R', 'L', 'P', 'F'],
            'G83' : ['Peck drill', 'A', 'R', 'L', 'Q', 'F'],
            'G73' : ['Chip-break drill', 'A', 'R', 'L', 'Q', 'F'],
            'G85' : ['Bore', 'A', 'R', 'L', 'F'],
            'G89' : ['Bore with dwell', 'A', 'R', 'L', 'P', 'F'],
            'G92' : ['Offset all coordinate systems', 'A'],
            'G96' : ['CSS Mode', 'S', 'D'],
            }

    def get_description(self, gcode):
        return self.codes[gcode][0]
    
    def get_words(self, gcode):
        self.gcode = gcode
        # strip description
        if not self.codes.has_key(gcode):
            return []
        words = self.codes[gcode][1:]
        # replace A with the real axis names
        if 'A' in words:
            i = words.index('A')
            words = words[:i] + self.axes + words[i+1:]
        return words

    def clear(self):
        self.words = {}

    def set_word(self, word, value):
        self.words[word] = value

    def issue(self):
        m = self.gcode
        for i in self.words:
            if len(self.words.get(i)) > 0:
                m += i + self.words.get(i)
        self.emccommand.mode(self.emc.MODE_MDI)
        self.emccommand.mdi(m)


class mdi_control:
    def __init__(self, gtk, emc, labels, eventboxes):
        self.labels = labels
        self.eventboxes = eventboxes
        self.numlabels = len(labels)
        self.numwords = 1
        self.selected = 0
        self.gtk = gtk
        
        self.mdi = mdi(emc)
        
        for i in range(self.numlabels):
            self.not_editing(i)
        self.editing(self.selected)
        self.set_text("G")
            
    def not_editing(self, n):
        e = self.eventboxes[n]
        e.modify_bg(self.gtk.STATE_NORMAL, self.gtk.gdk.color_parse("#ccc"))

    def editing(self, n):
        self.not_editing(self.selected)
        self.selected = n
        e = self.eventboxes[n]
        e.modify_bg(self.gtk.STATE_NORMAL, self.gtk.gdk.color_parse("#fff"))

    def get_text(self):
        w = self.labels[self.selected]
        return w.get_text()

    def set_text(self, t, n = -1):
        if n == -1: n = self.selected
        w = self.labels[n]
        w.set_text(t)
        if n > 0:
            if len(t) > 1:
                self.mdi.set_word(t[0], t[1:])
            if len(t) == 1:
                self.mdi.set_word(t, "")
        if len(t) < 2:
            w.modify_fg(self.gtk.STATE_NORMAL, self.gtk.gdk.color_parse("#888"))
        else:
            w.modify_fg(self.gtk.STATE_NORMAL, self.gtk.gdk.color_parse("#000"))
            
    def clear(self, b):
        t = self.get_text()
        self.set_text(t[:1])
        
    def back(self, b):
        t = self.get_text()
        if len(t) > 1:
            self.set_text(t[:-1])

    def fill_out(self):
        if self.selected == 0:
            w = self.mdi.get_words(self.get_text())
            self.numwords = len(w)
            for i in range(1,self.numlabels):
                if i <= len(w):
                    self.set_text(w[i-1], i)
                else:
                    self.set_text("", i)

    def next(self, b):
        self.fill_out();
        if self.numwords > 0:
            self.editing(max(1,(self.selected+1) % (self.numwords+1)))

    def ok(self, b):
        self.fill_out();
        self.mdi.issue()

    def decimal(self, b):
        t = self.get_text()
        if t.find(".") == -1:
            self.set_text(t + ".")

    def minus(self, b):
        t = self.get_text()
        if self.selected > 0:
            if t.find("-") == -1:
                self.set_text(t[:1] + "-" + t[1:])
            else:
                self.set_text(t[:1] + t[2:])

    def keypad(self, b):
        t = self.get_text()
        num = b.get_name()
        self.set_text(t + num)

    def g(self, b, code="G"):
        self.set_text(code, 0)
        for i in range(1, self.numlabels):
            self.set_text("", i)
        self.editing(0)
        self.mdi.clear()

    def m(self, b):
        self.g(b, "M")

    def select(self, eventbox, event):
        n = int(eventbox.get_name()[12:])
        if self.selected == 0:
            self.fill_out()
        if n <= self.numwords:
            self.editing(n)
