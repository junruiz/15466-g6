# Pac Man but you might get eaten by other Pac Man

Author: Junrui Zhao, Sizhe Chen

Design: Predator vs prey, try to eat consumables and get as much points as the prey. When you are the predator, try to catch other
players for points. Predator/Prey switch every 15 sec.

Note: The game is designed to finish within 60 sec and 2 players.

Networking:
The network structure is similar to the base code. We also include score/player_mode... information into the message so PlayMode can render
the corresponding component.

Screen Shot:

![Screen Shot](screenshot.png)

How To Play:

WASD to move your character.

Sources: Base code characters

Extra feature：Different player mode display differently! When you are predator you get a fork. When you are ready/invincible mode, you get to blink!

This game was built with [NEST](NEST.md).
