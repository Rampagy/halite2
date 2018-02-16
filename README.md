# halite2

This AI placed 58 in the [halite 2 competition](https://halite.io/user/?user_id=1401) under the username Rampagy.

## Outline

This AI was built in 4 sections:

1. Harass
2. Defend
3. Attack
4. Capture

### Harass

The point of harassing is to waste as much time of the enemy units as possible.  To do this I go towards the closest captured enemy planet (or closest enemy if there are no captured planets) and attack their docked ships.  If an undocked enemy ship gets within XX distance of my harass ship I then start evading.

Evasion is the key to harass and is what keeps my harassing ship alive.  Once I determine that I need to evade I look at all 360 degrees around my ship and pick the lowest score (score = enemy ships within attacking distance * distance from targeted harass planet).  By picking the lowest score I was able do a 'drive by shooting' while still evading the enemy ships.

### Defending

I decided to defend because it allowed me to protect my docked units from attackers without wasting my units by chasing their evading units.  When defending I would take 1 unit (only 1 unit per planet can defend) and put it between the enemy closest to planet and the closest docked ship.  This created a cool effect where my defending ship would follow an attacking ship around the planet while always being between it and the docked ships.

### Attacking

I used attacking to not only thin the other players forces but to also lead my units to planets that it may not have otherwise gone to.  I did this by having my attacking unit go towards the closest enemy unit as long as it was within NN distance of my ship.  This was cool because it would intercept ships on the way to my planets which means it would 'follow the trail' of ships back their originating planets (I think this was a big reason why all of my bots that had attack in them performed better than the ones without attack).

### Capture

This simply used a scoring algorithm to evaluate based on the ship which planet it should try to capture and then go towards the planet.

### Miscellaneous

1. To prevent my units from colliding I did some cool stuff that I won't explain here but is contained in [navigation](hlt/navigation.hpp) if you want to take a look. Hint: crossing_paths function 
2. The only thing I did to improve my navigation was to have it search on positive and negative angle of the target angle instead of only positive.  This prevented the weird whirlpooling around planets seen by some bots.  This code is located in [navigation](hlt/navigation.hpp).
