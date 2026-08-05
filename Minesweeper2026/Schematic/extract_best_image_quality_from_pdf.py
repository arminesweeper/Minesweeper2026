import fitz
path = r'e:\Robotics_Team\Minesweeper\Minesweeper\Minesweeper2026\Schematic\mine_sch.pdf'
doc = fitz.open(path)
page = doc[0]
print('page size', page.rect)
mat = fitz.Matrix(4, 4)
pix = page.get_pixmap(matrix=mat)
out = r'e:\Robotics_Team\Minesweeper\Minesweeper\Minesweeper2026\Schematic\mine_sch_hires.png'
pix.save(out)
print('saved', out, pix.width, pix.height)
