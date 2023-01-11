from geometry import Polygon, Line
import pygame

line = Line((10, 10), (60, 60))
circle = line.as_cirle()
dis = pygame.display.set_mode((600, 600))
while True:
     dis.fill((0, 0, 0))
     pygame.draw.circle(dis, (255, 0, 0), circle, 1)

     pygame.draw.line(dis, (255, 0, 0), line.a, line.b)
     pygame.display.update()