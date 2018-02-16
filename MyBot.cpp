#include <stdio.h>
#include <map>
#include <cmath>

#include "hlt/hlt.hpp"
#include "hlt/navigation.hpp"



hlt::Location find_resulting_location(hlt::Location start, double angle, int thrust){
    start.pos_x += thrust * cos( angle * M_PI / 180.);
    start.pos_y += thrust * sin( angle * M_PI / 180.);
    
    return start;
}




int main() {
    // Create bot name
    const hlt::Metadata metadata = hlt::initialize("Rampagy");
    const hlt::PlayerId my_id = metadata.player_id;

    std::vector<hlt::Move> moves;
    std::vector<hlt::Ship> ships_currently_harassing;
    
    int turn_count = 0;
    
    while(true) {
        moves.clear();
        
        // update the map
        hlt::Map hal_map = hlt::in::get_map(metadata.map_width, metadata.map_height);
        hlt::Log::log("Turn: " + std::to_string(turn_count));
        
        // create set of my ships that can be modified
        std::vector<hlt::Ship> my_ships = hal_map.ships.at(my_id);
        
        // create set of my ships that can be modified
        std::vector<hlt::Ship> exclude_ships;
        
        // create map to determine to count how many ships are sent to each planet
        std::map<hlt::EntityId, int> ShipsSentTo;
        
        // track the value of each ship to each planet, ShipValues[ship_id][planet_id] = value
        std::map<hlt::EntityId, std::map<hlt::EntityId, double>> ShipValues;
        
        std::vector<hlt::Ship> enemy_ships;
        
        // for each player in the game
        for (const auto& game_player : hal_map.ships){
            // if the player is not me
            if (game_player.first != my_id){
                // collect their ships
                for (const auto& enemy_ship : game_player.second){
                    enemy_ships.insert(enemy_ships.end(), enemy_ship);
                }
            }
        }
        
        std::vector<hlt::Ship> my_undocked_ships;
        // determine which of my ships can harass
        for (const hlt::Ship& my_ship : my_ships){
            // intialize the future position to its current location
            hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(my_ship.entity_id)).future_location.pos_x = my_ship.location.pos_x;
            hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(my_ship.entity_id)).future_location.pos_y = my_ship.location.pos_y;
            
            if (my_ship.docking_status != hlt::ShipDockingStatus::Undocked){
                // skip the ship if it is docked
                continue;
            }
            
            my_undocked_ships.push_back(hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(my_ship.entity_id)));
        }
        
        hlt::Location my_ave_location = {0, 0};
        int my_planet_count = 0;
        
        // find my average planet location (if no owned planets use ships)
        for (const hlt::Planet& planet : hal_map.planets){
            if (planet.owner_id == my_id){
                my_ave_location.pos_x += planet.location.pos_x;
                my_ave_location.pos_y += planet.location.pos_y;
                my_planet_count++;
            }
        }
        
        // if there were no planets use my ships as my average position instead
        if (my_planet_count == 0){
            for(const hlt::Ship& my_ship : my_ships){
                my_ave_location.pos_x += my_ship.location.pos_x;
                my_ave_location.pos_y += my_ship.location.pos_y;
                my_planet_count++;
            }
        }
        
        // take the average
        my_ave_location.pos_x /= my_planet_count;
        my_ave_location.pos_y /= my_planet_count;
        
        
        // before harassing and adding to exclude list check to see if any of the harassing ships have died 
        // if they have then pop them off of the ships_currently_harassing list
        // also update the position of all the harassing ships
        for (const hlt::Ship& ship_harassing : ships_currently_harassing){
            for (const hlt::Ship& my_ship : my_ships){
                if (my_ship.entity_id == ship_harassing.entity_id){
                    // if the ship is still alive add it to the exclude ships
                    // MUST MAKE SURE IT IS my_ship OTHERWISE THE SHIPS LOCATION WON'T UPDATE
                    exclude_ships.push_back(my_ship);
                    break;
                }
            }
        }
        ships_currently_harassing.clear();
        // at this point in time exclude_ships is a list of all the alive ships to harass, so replace ships_currently_harassing with exclude_ships
        ships_currently_harassing = exclude_ships;
        
        
        /********************* HARASS ENEMIES ********************/
        // determine how many of my ships should be harassing (X% of my undocked ships should be harassing or N ships, whichever is less)
        int ships_to_harass = (int)ceil(0.07*(double)my_undocked_ships.size());
        ships_to_harass = (ships_to_harass > 3) ? 3 : ships_to_harass;
        
        bool enemy_planets_found = false;
        
        hlt::Planet best_planet;
        std::map<hlt::EntityId, double> planet_scores;
        double min_score = 9999999;
        
        // determine where to harass (planet or ships if no planets are owned)
        for (const hlt::Planet& planet : hal_map.planets){
            if (!planet.owned || (planet.owner_id == my_id)){
                // skip the planet if it is not owned or I own it
                continue;
            }
            
            double score = my_ave_location.get_distance_to(planet.location);
            
            enemy_planets_found = true;
            planet_scores[planet.entity_id] = score;
            
            if (score < min_score){
                min_score = score;
                best_planet = planet;
            }
        }
        
        // if there are planets to harass
        if (enemy_planets_found){
            
            // if there are not enough ships harassing add ships until you reach the threshold
            while (ships_currently_harassing.size() < ships_to_harass){
                double min_dist = 999999;
                hlt::Ship closest_ship;
                bool ship_available_to_move = false;
                
                // now go through each ship and find the closest one to the planet
                for (const hlt::Ship& my_ship : my_undocked_ships){
                    // skip the ship if it is in the exclude list
                    bool ship_is_excluded = false;
                    for (const hlt::Ship& exclude_ship : exclude_ships){
                        if (exclude_ship.entity_id == my_ship.entity_id){
                            ship_is_excluded = true;
                            break;
                        }
                    }
                    // skip the ship if it is in the exclude list
                    if (ship_is_excluded){
                        continue;
                    }
                    
                    double dist = my_ship.location.get_distance_to(my_ship.location.get_closest_point(best_planet.location, best_planet.radius));
                    
                    // check to see if it is the closest ship the best planet
                    if (dist < min_dist){
                        min_dist = dist;
                        closest_ship = my_ship;
                        ship_available_to_move = true;
                    }
                }
                
                if (ship_available_to_move){
                    // need to add to the exclude list as well as the harassing list
                    ships_currently_harassing.push_back(closest_ship);
                    exclude_ships.push_back(closest_ship);
                }
                else {
                    // there are no more available ships to move (it's impossible for us to meet our quota of 'ships_to_harass')
                    break;
                }
            }
            
            
            for (const hlt::Ship& my_harass_ship : ships_currently_harassing){
                bool do_evasion = false;
                double closest_enemy_angle = 180;
                double min_dist = 16;
                int ships_following = 0;
                
                // find the angle away from the closest enemy
                for (const hlt::Ship& enemy_ship : enemy_ships){
                    if (enemy_ship.docking_status != hlt::ShipDockingStatus::Undocked){
                        // skip the ship if it is not undocked
                        continue;
                    }
                    
                    // get dist between the enemy ships and my ship
                    double dist = enemy_ship.location.get_distance_to(enemy_ship.location.get_closest_point(my_harass_ship.location, my_harass_ship.radius));
                    
                    if (dist < 27){
                        ships_following++;
                    }
                    
                    // if the enemy is within X units of my ship do avoidance
                    if (dist < min_dist){
                        min_dist = dist;
                        do_evasion = true;
                        // add 180 because we want to go AWAY from the enemy, not towards
                        closest_enemy_angle = fmod(180 + atan2(enemy_ship.location.pos_y - my_harass_ship.location.pos_y, enemy_ship.location.pos_x - my_harass_ship.location.pos_x) * 180 / M_PI, 360);
                    }
                }
                
                double min_score = 9999999;
                // reevaluate the best planet for the ship to go to
                for(const auto& planet_pair : planet_scores){
                    hlt::Planet temp_planet = hal_map.get_planet(planet_pair.first);
                    double tmp_score = 0;
                    // average the two 'scores' together to get the best one 
                    if (my_planet_count/hal_map.planets.size() < 0.30) {
                        tmp_score = (planet_pair.second + my_harass_ship.location.get_distance_to(my_harass_ship.location.get_closest_point(temp_planet.location, temp_planet.radius)));
                    }
                    else {
                        tmp_score = 1 / (planet_pair.second + my_harass_ship.location.get_distance_to(my_harass_ship.location.get_closest_point(temp_planet.location, temp_planet.radius)));
                    }
                    
                    if (tmp_score < min_score){
                        min_score = tmp_score;
                        best_planet = temp_planet;
                    }
                }
                
                // now we have the closest ship to the best planet, so send the ship to one of the docked ships
                const hlt::Ship enemy_docked_ship = hal_map.get_ship(best_planet.owner_id, best_planet.docked_ships.front());
                hlt::Location target = my_harass_ship.location.get_closest_point(enemy_docked_ship.location, enemy_docked_ship.radius);
                
                // if we have some ships following me, perform evasion
                if (do_evasion){
                    int harass_speed = 7;
                    std::map<int, double> angle_score;
                    
                    // do an XX degree sweep around 'closest_enemy_angle' +/- XX degrees on each side
                    for (double new_angle = closest_enemy_angle-90; new_angle <= closest_enemy_angle+90; new_angle++ ){
                        // find the resulting location of the move
                        hlt::Location result = find_resulting_location(my_harass_ship.location, new_angle, harass_speed);
                        int enemy_future_attack_score = 0;
                        
                        for(const hlt::Ship& enemy_ship : enemy_ships){
                            if (enemy_ship.docking_status != hlt::ShipDockingStatus::Undocked){
                                // skip the ship if it is not undocked
                                continue;
                            }
                            
                            // get dist to the best planet
                            double dist = enemy_ship.location.get_distance_to(result);
                            
                            // check to see if the ship could attack the harassing ship (plus some fudge)
                            if (dist <= (hlt::constants::WEAPON_RADIUS + hlt::constants::MAX_SPEED + 1)){
                                enemy_future_attack_score += 5;
                            }
                            // check to see if the ship could attack within 2 turns (plus some fudge)
                            else if (dist <= (hlt::constants::WEAPON_RADIUS + 2*hlt::constants::MAX_SPEED + 1)){
                                enemy_future_attack_score += 1;
                            }
                        }
                        
                        double target_dist = result.get_distance_to(target);
                        
                        // look one step into the future for obstacles between
                        hlt::Location result_future = find_resulting_location(my_harass_ship.location, new_angle, harass_speed+2);
                        
                        // it is a valid location if the location keeps me out of attack range, and there are no objects in the way
                        // and if the location is inbounds of the map
                        if ((result.pos_x < (hal_map.map_width - 3)) && (result.pos_y < (hal_map.map_height - 3)) && 
                            (result.pos_x > 2) && (result.pos_y > 2) &&
                            hlt::navigation::objects_between(hal_map, my_harass_ship.location, result).empty() && 
                            hlt::navigation::crossing_paths(hal_map, my_harass_ship, result_future).empty()){
                                // keep track of each angle's score so that later we can pick the lowest score
                                angle_score[new_angle] = pow(target_dist/18, 2) - 2*ships_following + enemy_future_attack_score;
                        }
                    }
                    
                    double min_score = 99999;
                    int chosen_angle = closest_enemy_angle;
                    
                    // go through all the angles and pick the best angle (if angle_score is empty use closest_enemy_angle)
                    for (const auto& angle_pair : angle_score){
                        if (angle_pair.second < min_score){
                            chosen_angle = angle_pair.first;
                            min_score = angle_pair.second;
                        }
                    }
                    
                    // add the move to the queue
                    moves.push_back(hlt::Move::thrust(my_harass_ship.entity_id, harass_speed, fmod(chosen_angle, 360)));
                    
                    // update the positions of the ship in the map
                    hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(my_harass_ship.entity_id)).future_location.pos_x += harass_speed * cos( fmod(chosen_angle, 360) * M_PI / 180.);
                    hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(my_harass_ship.entity_id)).future_location.pos_y += harass_speed * sin( fmod(chosen_angle, 360) * M_PI / 180.);
                    
                    // no needed to put on exclude list (already on there)
                }
                else {
                    const hlt::possibly<hlt::Move> move = hlt::navigation::navigate_ship_towards_target(hal_map, my_harass_ship, 
                                    target, hlt::constants::MAX_SPEED, true, hlt::constants::MAX_NAVIGATION_CORRECTIONS, 1);
                    
                    // second index of the tuple indicates whether a valid path was found
                    if (move.second) {
                        // if a valid path was found enter it into the move queue
                        moves.push_back(move.first);
                        
                        // update the positions of the ship in the map
                        hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(my_harass_ship.entity_id)).future_location.pos_x += move.first.move_thrust * cos( move.first.move_angle_deg * M_PI / 180.);
                        hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(my_harass_ship.entity_id)).future_location.pos_y += move.first.move_thrust * sin( move.first.move_angle_deg * M_PI / 180.);
                        
                        // no needed to put on exclude list (already on there)
                    }
                }
            }
        }
        // if there are no enemy planets to go after, go towards the closest enemy ship (should only be true at beginning of game when no planets are owned)
        else {
            if (!ships_currently_harassing.empty() || !my_undocked_ships.empty()){
                std::map<hlt::EntityId, double> ship_to_enemy_distance_score;
                hlt::Ship min_ship;
                
                if (ships_currently_harassing.empty()){
                    // find the closest enemy to my closest ship and send my ship to that enemy up to an amount 'ships_to_harass'
                    for (const hlt::Ship& my_ship : my_undocked_ships){
                        for (const hlt::Ship& enemy_ship : enemy_ships){
                            ship_to_enemy_distance_score[my_ship.entity_id] = my_ship.location.get_distance_to(my_ship.location.get_closest_point(enemy_ship.location, enemy_ship.radius));
                        }
                    }
                    
                    double min_dist = 999999;
                    
                    for (const auto& ship_pair : ship_to_enemy_distance_score){
                        if (ship_pair.second < min_dist){
                            min_dist = ship_pair.second;
                            min_ship = hal_map.get_ship(my_id, ship_pair.first);
                        }
                    }
                }
                // there is at least one ship in ships_currently_harassing
                else {
                    min_ship = ships_currently_harassing.front();
                }
                
                
                hlt::Ship min_enemy;
                double min_dist = 999999;
                
                // find the closest enemy to the ship
                for (const hlt::Ship& enemy_ship : enemy_ships){
                    double dist = min_ship.location.get_distance_to(min_ship.location.get_closest_point(enemy_ship.location, enemy_ship.radius));
                    
                    if (dist < min_dist){
                        min_dist = dist;
                        min_enemy = enemy_ship;
                    }
                }
                
                hlt::Location target = min_ship.location.get_closest_point(min_enemy.location, min_enemy.radius);
                // now navigate towards that enemy
                const hlt::possibly<hlt::Move> move = hlt::navigation::navigate_ship_towards_target(hal_map, min_ship, 
                                        target, hlt::constants::MAX_SPEED, true, hlt::constants::MAX_NAVIGATION_CORRECTIONS, 1);
                        
                // second index of the tuple indicates whether a valid path was found
                if (move.second) {
                    // if a valid path was found enter it into the move queue
                    moves.push_back(move.first);
                    
                    // update the positions of the ship in the map
                    hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(min_ship.entity_id)).future_location.pos_x += move.first.move_thrust * cos( move.first.move_angle_deg * M_PI / 180.);
                    hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(min_ship.entity_id)).future_location.pos_y += move.first.move_thrust * sin( move.first.move_angle_deg * M_PI / 180.);
                    
                    if(ships_currently_harassing.empty()){
                        // put on the exclude_ships and the ships_currently_harassing lists only if it is currently not on them
                        exclude_ships.push_back(min_ship);
                        ships_currently_harassing.push_back(min_ship);
                    }
                }
            }
        }
        
        /******************* END HARASS ENEMIES ******************/
        
        
        
        
        
        /******************** DEFEND PLANETS *********************/
        // for each planet
        for (const hlt::Planet& planet : hal_map.planets){
            // if the owner is not me skip it
            if (planet.owner_id != my_id){
                continue;
            }
            
            for (const hlt::Ship& enemy_ship : enemy_ships){
                if (enemy_ship.docking_status != hlt::ShipDockingStatus::Undocked){
                    // skip the enemy ship if it is anything other than undocked
                    continue;
                }
                
                double min_docked_ship_to_enemy_dist = 999999;
                hlt::Ship closest_docked_ship_to_enemy;
                
                for (const auto& my_friendly_docked_ship_id : planet.docked_ships){
                    const hlt::Ship my_docked_ship = hal_map.get_ship(planet.owner_id, my_friendly_docked_ship_id);
                    // check to see if the ship is within defending range of my planet
                    double enemy_dist_from_docked_friendly = sqrt(pow(enemy_ship.location.pos_x - my_docked_ship.location.pos_x, 2) + pow(enemy_ship.location.pos_y - my_docked_ship.location.pos_y, 2));
                    
                    if (enemy_dist_from_docked_friendly < min_docked_ship_to_enemy_dist){
                        min_docked_ship_to_enemy_dist = enemy_dist_from_docked_friendly;
                        closest_docked_ship_to_enemy = my_docked_ship;
                    }
                }
                
                // if my docked ship is near the enemy protect the planet with my closest ship
                if (min_docked_ship_to_enemy_dist < 20){
                    double min_dist = 999999;
                    hlt::Ship min_friendly_ship;
                    
                    // find my closest ship to the docked ship
                    for (const auto& my_ship : my_undocked_ships){
                        
                        // skip the ship if it is in the exclude list
                        bool ship_is_excluded = false;
                        for (const hlt::Ship& exclude_ship : exclude_ships){
                            if (exclude_ship.entity_id == my_ship.entity_id){
                                ship_is_excluded = true;
                                break;
                            }
                        }
                        // skip the ship if it is in the exclude list
                        if (ship_is_excluded){
                            continue;
                        }
                        
                        double friendly_dist_to_docked = sqrt(pow(closest_docked_ship_to_enemy.location.pos_x - my_ship.location.pos_x, 2) + pow(closest_docked_ship_to_enemy.location.pos_y - my_ship.location.pos_y, 2));
                        
                        if (friendly_dist_to_docked < min_dist){
                            min_dist = friendly_dist_to_docked;
                            min_friendly_ship = my_ship;
                        }
                    }
                    
                    if (min_dist < 35){
                        // send my closest ship to attack the enemy
                        hlt::Location target = enemy_ship.location.get_closest_point(closest_docked_ship_to_enemy.location, closest_docked_ship_to_enemy.radius);
                        
                        const hlt::possibly<hlt::Move> move = hlt::navigation::navigate_ship_towards_target(hal_map, min_friendly_ship, 
                                    target, hlt::constants::MAX_SPEED, true, hlt::constants::MAX_NAVIGATION_CORRECTIONS, 1);
                        
                        // second index of the tuple indicates whether a valid path was found
                        if (move.second) {
                            // if a valid path was found enter it into the move queue
                            moves.push_back(move.first);
                            
                            // update the positions of the ship in the map
                            hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(min_friendly_ship.entity_id)).future_location.pos_x += move.first.move_thrust * cos( move.first.move_angle_deg * M_PI / 180.);
                            hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(min_friendly_ship.entity_id)).future_location.pos_y += move.first.move_thrust * sin( move.first.move_angle_deg * M_PI / 180.);
                            
                            // pop my ship off the my_ships vector so I don't accidently give it two commands
                            exclude_ships.push_back(min_friendly_ship);
                            
                            break;
                        }
                    }
                }
            }
        }
        /****************** END DEFEND PLANETS ********************/
        
        
        /************************ ATTACK *************************/
        for (const hlt::Ship& my_ship : my_undocked_ships){
            
            // skip the ship if it is in the exclude list
            bool ship_is_excluded = false;
            for (const hlt::Ship& exclude_ship : exclude_ships){
                if (exclude_ship.entity_id == my_ship.entity_id){
                    ship_is_excluded = true;
                    break;
                }
            }
            // skip the ship if it is in the exclude list
            if (ship_is_excluded){
                continue;
            }
            
            const double ave_planet_dist = my_ship.location.get_distance_to(my_ave_location);
            
            // find the closest enemy ship
            double min_enemy_dist = 9999999;
            hlt::Ship min_enemy_ship;
            
            for (const auto& enemy_ship : enemy_ships){
                // skip the ship if it isn't undocked
                if (enemy_ship.docking_status != hlt::ShipDockingStatus::Undocked){
                    continue;
                }
                
                // find the distance to the ship
                const double enemy_ship_dist = enemy_ship.location.get_distance_to(my_ship.location);
                const double enemy_ship_dist_from_my_ave_loc = enemy_ship.location.get_distance_to(my_ave_location);
                
                // only a viable ship to attack if it would be getting farther from my location
                if ((enemy_ship_dist < min_enemy_dist) && (enemy_ship_dist_from_my_ave_loc > ave_planet_dist)){
                    min_enemy_dist = enemy_ship_dist;
                    min_enemy_ship = enemy_ship;
                }
            }
            
            // only attack if the ship if it is within 20 units of my ship and the attack would bring it farther from my average planet location
            if (min_enemy_dist < 25){
                // navigate/attack that bitch
                hlt::Location target = my_ship.location.get_closest_point(min_enemy_ship.location, min_enemy_ship.radius);
                
                const hlt::possibly<hlt::Move> move = hlt::navigation::navigate_ship_towards_target(hal_map, my_ship, 
                            target, hlt::constants::MAX_SPEED, true, hlt::constants::MAX_NAVIGATION_CORRECTIONS, 1);
                
                // second index of the tuple indicates whether a valid path was found
                if (move.second) {
                    // if a valid path was found enter it into the move queue
                    moves.push_back(move.first);
                    
                    // update the positions of the ship in the map
                    hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(my_ship.entity_id)).future_location.pos_x += move.first.move_thrust * cos( move.first.move_angle_deg * M_PI / 180.);
                    hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(my_ship.entity_id)).future_location.pos_y += move.first.move_thrust * sin( move.first.move_angle_deg * M_PI / 180.);
                    
                    // pop my ship off the my_ships vector so I don't accidently give it two commands
                    exclude_ships.push_back(my_ship);
                }
            }
        }
        /********************* END ATTACK ************************/
        
        
        /******************* PLANET CAPTURE **********************/
        // find the best ship to go to each planet
        for (const hlt::Ship& my_ship : my_undocked_ships){
            
            // skip the ship if it is in the exclude list
            bool ship_is_excluded = false;
            for (const hlt::Ship& exclude_ship : exclude_ships){
                if (exclude_ship.entity_id == my_ship.entity_id){
                    ship_is_excluded = true;
                    break;
                }
            }
            // skip the ship if it is in the exclude list
            if (ship_is_excluded){
                continue;
            }
            
            double min_enemy_dist = 999999;
            // for each one of my ships find the nearest enemy
            for (const auto& enemy_ship : enemy_ships){
                if (enemy_ship.docking_status != hlt::ShipDockingStatus::Undocked){
                    // skip the enemy ship if it is anything other than undocked
                    continue;
                }
                
                double my_ship_dist_to_enemy = my_ship.location.get_distance_to(my_ship.location.get_closest_point(enemy_ship.location, enemy_ship.radius));
                if (my_ship_dist_to_enemy < min_enemy_dist){
                    min_enemy_dist = my_ship_dist_to_enemy;
                }
            }
            
            
            for (const hlt::Planet& planet : hal_map.planets){
                // if the planet is already full and I own it
                if (planet.is_full() && (planet.owner_id == my_id)){
                    // skip the planet
                    continue;
                }
                
                // get the ships value to this planet, lower is better (value = distance / 1.5^x)
                double planet_value = my_ship.location.get_distance_to(my_ship.location.get_closest_point(planet.location, planet.radius)); // pow(1.4, planet.docking_spots);
                
                // insert the ships value to the planet
                ShipValues[my_ship.entity_id][planet.entity_id] = planet_value;
            }
        }
        
        // now that we know every ships value find the lowest value for each ship (assuming the planet isn't full)
        for (const auto& my_ship : ShipValues){ // for each ship
            hlt::Planet min_planet;
            double min_value = 999999;
            
            for (const auto& planet : my_ship.second){ // for each planet
                hlt::Planet planet_obj = hal_map.get_planet(planet.first);
                
                // if the planet is not owned or I own it.
                if (!planet_obj.owned || (planet_obj.owner_id == my_id)){
                    // grab iterator of the planet
                    std::map<hlt::EntityId, int>::iterator planet_iter = ShipsSentTo.find(planet_obj.entity_id);
                    
                    if (planet_iter != ShipsSentTo.end()){
                        if ( planet_iter->second >= (planet_obj.docking_spots - planet_obj.docked_ships.size()) ){
                            // skip the planet if we have already sent enough ships there to fully occupy it
                            continue;
                        }
                    }
                }
                
                // if the planet is the closest/best for the ship
                if (planet.second < min_value){
                    min_value = planet.second;
                    min_planet = planet_obj;
                }
            }
            
            hlt::Ship ship = hal_map.get_ship(my_id, my_ship.first);
            // if the planet can be docked
            if (ship.can_dock(min_planet) &&
                (((min_planet.docked_ships.size() == 0) && (min_planet.owner_id != my_id)) || 
                ((min_planet.docked_ships.size() < min_planet.docking_spots) && (min_planet.owner_id == my_id)))){
                    
                // then dock
                moves.push_back(hlt::Move::dock(ship.entity_id, min_planet.entity_id));
                
                // track which ships are being docked
                ShipsSentTo[min_planet.entity_id]++;
                
                // DOCKING DOES NOT CHANGE A SHIPS LOCATION thusly there is no need to udpate the ships location
            }
            // if I don't own the planet and there are ships docked
            else if ( min_planet.owned && (min_planet.owner_id != my_id) && (min_planet.docked_ships.size() > 0) ){
                // navigate/attack the closest docked ship
                double min_dist = 999999;
                hlt::Ship closest_docked_enemy;
                
                // find the closest docked ship
                for (const hlt::EntityId& docked_enemy_id : min_planet.docked_ships){
                    // get the enemy ship
                    const hlt::Ship docked_enemy = hal_map.get_ship(min_planet.owner_id, docked_enemy_id);
                    
                    double docked_enemy_dist = sqrt( pow(docked_enemy.location.pos_x - ship.location.pos_x, 2) + pow(docked_enemy.location.pos_y - ship.location.pos_y, 2) );
                    
                    if (docked_enemy_dist < min_dist){
                        min_dist = docked_enemy_dist;
                        closest_docked_enemy = docked_enemy;
                    }
                }
                
                hlt::Location target = ship.location.get_closest_point(closest_docked_enemy.location, closest_docked_enemy.radius);
                
                const hlt::possibly<hlt::Move> move = hlt::navigation::navigate_ship_towards_target(hal_map, ship, target, hlt::constants::MAX_SPEED, true, 
                                                                                                    hlt::constants::MAX_NAVIGATION_CORRECTIONS, 1);
                
                // DONT CHECK FOR LEGITIMATE PATH BECAUSE IF NO PATH IS FOUND WE DONT WANT TO MOVE -> ship.noop()
                // add move to the command queue
                moves.push_back(move.first);
                
                // update the positions of the ship in the map
                hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(ship.entity_id)).future_location.pos_x += move.first.move_thrust * cos( move.first.move_angle_deg * M_PI / 180.);
                hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(ship.entity_id)).future_location.pos_y += move.first.move_thrust * sin( move.first.move_angle_deg * M_PI / 180.);
            }
            // navigate to the closest planet
            else {
                // navigate to the closest planet
                const hlt::possibly<hlt::Move> move = hlt::navigation::navigate_ship_to_dock(hal_map, ship, min_planet, hlt::constants::MAX_SPEED);
                
                // DONT CHECK FOR LEGITIMATE PATH BECAUSE IF NO PATH IS FOUND WE DONT WANT TO MOVE -> ship.noop()
                // add move to the command queue
                moves.push_back(move.first);
                
                // track which ships are going where
                ShipsSentTo[min_planet.entity_id]++;
                
                // update the positions of the ship in the map
                hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(ship.entity_id)).future_location.pos_x += move.first.move_thrust * cos( move.first.move_angle_deg * M_PI / 180.);
                hal_map.ships.at(my_id).at(hal_map.ship_map.at(my_id).at(ship.entity_id)).future_location.pos_y += move.first.move_thrust * sin( move.first.move_angle_deg * M_PI / 180.);
            }
            /**************** END PLANET CAPTURE *****************/
        }
        
        
        // send moves at the end of the turn
        if (!hlt::out::send_moves(moves)) {
            hlt::Log::log("send_moves failed; exiting");
            break;
        }
        turn_count++;
    }
}
