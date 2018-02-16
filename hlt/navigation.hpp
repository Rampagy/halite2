#pragma once

#include <stdio.h>

#include "collision.hpp"
#include "map.hpp"
#include "move.hpp"
#include "util.hpp"

namespace hlt {
    namespace navigation {
        static void check_and_add_entity_between(
                std::vector<const Entity *>& entities_found,
                const Location& start,
                const Location& target,
                const Entity& entity_to_check)
        {
            const Location &location = entity_to_check.location;
            if (location == start || location == target) {
                return;
            }
            if (collision::segment_circle_intersect(start, target, entity_to_check, constants::FORECAST_FUDGE_FACTOR)) {
                entities_found.push_back(&entity_to_check);
            }
        }

        static std::vector<const Entity *> objects_between(const Map& map, const Location& start, const Location& target) {
            std::vector<const Entity *> entities_found;

            for (const Planet& planet : map.planets) {
                check_and_add_entity_between(entities_found, start, target, planet);
            }

            for (const auto& player_ship : map.ships) {
                for (const Ship& ship : player_ship.second) {
                    check_and_add_entity_between(entities_found, start, target, ship);
                }
            }

            return entities_found;
        }
        
        static std::vector<const Entity *> crossing_paths(const Map& map, const Ship& move_ship, const Location& move_loc) {
            std::vector<const Entity *> entities_found;

            // find slope and intersept for the move_ship
            double m = (move_loc.pos_y - move_ship.location.pos_y) / (move_loc.pos_x - move_ship.location.pos_x);
            double b = move_ship.location.pos_y - move_ship.location.pos_x * m;
            
            // go through each of my ships and find if there are any crossing paths
            for (const Ship& my_ship : map.ships.at(move_ship.owner_id)) {
                if ((move_ship.entity_id == my_ship.entity_id) || (my_ship.docking_status != ShipDockingStatus::Undocked)){
                    // skip the ship if it is the move_ship or it is not undocked
                    continue;
                }
                
                // if the future location and the current location are the same, the ship has not queued a move yet, so skip it
                if (my_ship.location == my_ship.future_location){
                    continue;
                }
                
                // find the slope and y-intercept for the ship we are trying to determine if we intersect
                double intersect_m = (my_ship.future_location.pos_y - my_ship.location.pos_y) / (my_ship.future_location.pos_x - my_ship.location.pos_x);
                double intersect_b = my_ship.location.pos_y - my_ship.location.pos_x * intersect_m;
                
                double intersect_x = (intersect_b - b) / (m - intersect_m);
                double intersect_y = m*intersect_x + b;
                
                // do the lines intersect in the segment that will be travel in the next turn?
                if ((intersect_x <= std::max(move_loc.pos_x, move_ship.location.pos_x) + 1) && 
                    (intersect_x >= std::min(move_loc.pos_x, move_ship.location.pos_x) - 1) &&
                    (intersect_y <= std::max(move_loc.pos_y, move_ship.location.pos_y) + 1) &&
                    (intersect_y >= std::min(move_loc.pos_y, move_ship.location.pos_y) - 1) &&
                    // to prevent false intersections that are 180 degrees away from direction of travel
                    (intersect_x <= std::max(my_ship.location.pos_x, my_ship.future_location.pos_x) + 1) && 
                    (intersect_x >= std::min(my_ship.location.pos_x, my_ship.future_location.pos_x) - 1) &&
                    (intersect_y <= std::max(my_ship.location.pos_y, my_ship.future_location.pos_y) + 1) &&
                    (intersect_y >= std::min(my_ship.location.pos_y, my_ship.future_location.pos_y) - 1)){
                        // if a collision is found no need to keep checking so stop the for loop
                        entities_found.push_back(&my_ship);
                        break;
                }
            }
            
            return entities_found;
        }
        
        static possibly<Move> navigate_ship_towards_target(
                const Map& map,
                const Ship& ship,
                const Location& target,
                const int max_thrust,
                const bool avoid_obstacles,
                const int max_corrections,
                const int angular_step_deg) // angular_step_rad
        {
            double distance = ship.location.get_distance_to(target);
            double angle_rad = ship.location.orient_towards_in_rad(target);
            
            Location new_target;
            bool path_found = false;
            
            // if you want to avoid obstacles and there are objects between
            if (avoid_obstacles && (!objects_between(map, ship.location, target).empty() || !crossing_paths(map, ship, target).empty())) {
                // while there is an obstacle between the ship and target
                for (int i = 0; i <= max_corrections; i+=angular_step_deg){
                    for (int j = 0; j < 2; j++){
                        int angle_mod = 0;
                        if (j == 0){
                            angle_mod = i;
                        } else {
                            angle_mod = -i;
                        }
                        
                        // recalculate the new target
                        double new_target_dx = cos(angle_rad + (angle_mod*M_PI)/180.0) * distance;
                        double new_target_dy = sin(angle_rad + (angle_mod*M_PI)/180.0) * distance;
                        new_target = { ship.location.pos_x + new_target_dx, ship.location.pos_y + new_target_dy };
                        
                        // if there are no obstacles in the path
                        if (objects_between(map, ship.location, new_target).empty() && crossing_paths(map, ship, new_target).empty()){
                            // recalculate the angle to the new target
                            angle_rad = ship.location.orient_towards_in_rad(new_target);
                            path_found = true;
                            break;
                        }
                    }
                    if (path_found){
                        break;
                    }
                }

                if (!path_found){
                    // if there are no solutions without obstacles in the way
                    return { Move::noop(), false };
                }
            }

            int thrust;
            if (distance < max_thrust) {
                // Do not round up, since overshooting might cause collisions.
                thrust = (int) distance;
            } else {
                thrust = max_thrust;
            }
            
            // convert from radians to degrees
            const int angle_deg = util::angle_rad_to_deg_clipped(angle_rad);

            return { Move::thrust(ship.entity_id, thrust, angle_deg), true };
        }

        static possibly<Move> navigate_ship_to_dock(
                const Map& map,
                const Ship& ship,
                const Entity& dock_target,
                const int max_thrust)
        {
            const int max_corrections = constants::MAX_NAVIGATION_CORRECTIONS;
            const bool avoid_obstacles = true;
            const Location& target = ship.location.get_closest_point(dock_target.location, dock_target.radius);

            return navigate_ship_towards_target(
                    map, ship, target, max_thrust, avoid_obstacles, max_corrections, 1);
        }
    }
}
