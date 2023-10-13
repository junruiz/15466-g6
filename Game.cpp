#include "Game.hpp"

#include "Connection.hpp"

#include <stdexcept>
#include <iostream>
#include <cstring>
#include "data_path.hpp"

#include <glm/gtx/norm.hpp>

#include <fstream>
#include <sstream>
#include <string>

void Player::Controls::send_controls_message(Connection *connection_) const {
	assert(connection_);
	auto &connection = *connection_;

	uint32_t size = 5;
	connection.send(Message::C2S_Controls);
	connection.send(uint8_t(size));
	connection.send(uint8_t(size >> 8));
	connection.send(uint8_t(size >> 16));

	auto send_button = [&](Button const &b) {
		if (b.downs & 0x80) {
			std::cerr << "Wow, you are really good at pressing buttons!" << std::endl;
		}
		connection.send(uint8_t( (b.pressed ? 0x80 : 0x00) | (b.downs & 0x7f) ) );
	};

	send_button(left);
	send_button(right);
	send_button(up);
	send_button(down);
	send_button(restart);
}

bool Player::Controls::recv_controls_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;

	auto &recv_buffer = connection.recv_buffer;

	//expecting [type, size_low0, size_mid8, size_high8]:
	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::C2S_Controls)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	if (size != 5) throw std::runtime_error("Controls message with size " + std::to_string(size) + " != 5!");
	
	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	auto recv_button = [](uint8_t byte, Button *button) {
		button->pressed = (byte & 0x80);
		uint32_t d = uint32_t(button->downs) + uint32_t(byte & 0x7f);
		if (d > 255) {
			std::cerr << "got a whole lot of downs" << std::endl;
			d = 255;
		}
		button->downs = uint8_t(d);
	};

	recv_button(recv_buffer[4+0], &left);
	recv_button(recv_buffer[4+1], &right);
	recv_button(recv_buffer[4+2], &up);
	recv_button(recv_buffer[4+3], &down);
	recv_button(recv_buffer[4+4], &restart);

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}


//-----------------------------------------

Game::Game() : mt(0x15466666) {
	load_map();
}

Player *Game::spawn_player() {
	players.emplace_back();
	Player &player = players.back();

	//random point in the middle area of the arena:
	if (players.size() == 1){
		player.position= player1_spawn;
	}else{
		player.position= player2_spawn;
	}

	do {
		player.color.r = mt() / float(mt.max());
		player.color.g = mt() / float(mt.max());
		player.color.b = mt() / float(mt.max());
	} while (player.color == glm::vec3(0.0f));
	player.color = glm::normalize(player.color);

	player.name = "Player " + std::to_string(next_player_number++);

	return &player;
}

void Game::remove_player(Player *player) {
	bool found = false;
	for (auto pi = players.begin(); pi != players.end(); ++pi) {
		if (&*pi == player) {
			players.erase(pi);
			found = true;
			break;
		}
	}
	assert(found);
}

void Game::update(float elapsed) {
	time += elapsed;
	if (mode == 0) {
		ready_seconds = 0;
		playing_seconds = 0;
		if (time > 1.0f) {
			time -= 1.0f;
		}
		//waiting for another player to join
		int joined = 0;
		for (auto &p : players) {
			if (p.ready) {
				joined ++;
			}
		}
		if (joined == 2) {
			mode = 1;
			for (auto &consumable :consumables) {
				consumable.consumed = false;
			}
		}
	}

	if (mode == 1) {
		//ready
		playing_seconds = 0;
		if (time > 1.0f){
			ready_seconds += 1;
			time -= 1.0f;
		}
		if (ready_seconds >= 10) {
			mode = 2;
			ready_seconds = 0;
		}
		for (auto &p : players) {
			p.death_time += elapsed;
			p.speed_sp = 0;
		}
	}

	if (mode == 2) {
		ready_seconds = 0;
		//playing
		if (time > 1.0f) {
			playing_seconds += 1;
			time -= 1.0f;
			for (auto &p: players) {
				p.speed_sp += p.speedsp_inc;
				if (p.speed_sp < 0) {
					p.speed_sp = 0;
				}
				if (p.speed_sp > 10) {
					p.speed_sp = 10;
				}
				p.speedsp_inc = 1;
			}
		}
		if (playing_seconds >= 60) {
			mode = 3;
			playing_seconds = 0;
			for (auto &p: players) {
				p.ready = false;
				p.mode = 0;
			}
		}
		change_predator_time += elapsed;
		if (change_predator_time >= 15.0f) {
			//change_predator
			if (predator == 1) {
				predator = 2;
			}
			else if (predator == 2) {
				predator = 1;
			}
			change_predator_time -= 15.0f;
		}
		int player_idx = 0;
		for (auto &p : players) {
			player_idx ++;
			if (p.mode == 0) {
				if (predator != player_idx) {
					p.mode = 2;
				}
				else if (predator == player_idx) {
					p.mode = 1;
					predator_name = p.name;
				}
				p.death_time = 0;
			}
			if (p.mode == 1) {
				if (predator != player_idx) {
					p.mode = 2;
				}
			}
			if (p.mode == 2) {
				if (predator == player_idx) {
					p.mode = 1;
					predator_name = p.name;
				}
			}
			if (p.mode == 3) {
				p.death_time += elapsed;
				if (predator != player_idx) {
					if (p.death_time > 5.0f) {
						p.mode = 2;
						p.death_time = 0;
					}
				}
				else if (predator == player_idx) {
					predator_name = p.name;
					if (p.death_time > 5.0f) {
						p.mode = 1;
						p.death_time = 0;
					}
				}
			}
		}
	}
	
	if (mode == 3) {
		for (auto &p: players) {
			p.mode = 0;
		}
		time = 0;
		playing_seconds = 0;
		ready_seconds = 0;
		predator_name = "";
	}

	//position/velocity update:
	for (auto &p : players) {
		glm::vec2 dir = glm::vec2(0.0f, 0.0f);
		if (p.controls.left.pressed) dir = glm::vec2(-1.0f - p.speed_sp * 0.1f, 0.0f);
		if (p.controls.right.pressed) dir = glm::vec2(1.0f + p.speed_sp * 0.1f, 0.0f);
		if (p.controls.down.pressed) dir = glm::vec2(0.0f, -1.0f - p.speed_sp * 0.1f);
		if (p.controls.up.pressed) dir = glm::vec2(0.0f, 1.0f + p.speed_sp * 0.1f);

		else if (dir != glm::vec2(0.0f, 0.0f)) {
			p.speedsp_inc = -1;
		}
		
		if ((mode == 3 || mode == 0) && p.controls.restart.pressed) {
			mode = 0;
			p.score = 0;
			p.velocity = glm::vec2(0.0f, 0.0f);
			p.mode = 0;
			p.death_time = 0;
			p.speed_sp = 0;
			p.ready = true;
		}

		if (dir == glm::vec2(0.0f)) {
			//no inputs: just drift to a stop
			// float amt = 1.0f - std::pow(0.5f, elapsed / (PlayerAccelHalflife * 2.0f));
			// p.velocity = glm::mix(p.velocity, glm::vec2(0.0f,0.0f), amt);
			p.velocity = glm::vec2(0.0f, 0.0f);
		} else {
			//inputs: tween velocity to target direction
			p.velocity = dir * 0.5f;
		}
		p.position += p.velocity * elapsed;
		//reset 'downs' since controls have been handled:
		p.controls.left.downs = 0;
		p.controls.right.downs = 0;
		p.controls.up.downs = 0;
		p.controls.down.downs = 0;
		p.controls.restart.downs = 0;
	}

	//collision resolution:
	for (auto &p1 : players) {
		//player/player collisions:
		for (auto &p2 : players) {
			if (&p1 == &p2) break;

			glm::vec2 p12 = p2.position - p1.position;
			float len2 = glm::length2(p12);
			if (len2 > (2.0f * PlayerRadius) * (2.0f * PlayerRadius)) continue;
			// if (len2 == 0.0f) continue;

			if (p1.mode == 1 && p2.mode == 2) {
				uint16_t get_score = p2.score - p2.score / 2;
				p2.score -= get_score;
				p1.score += get_score;
				p2.mode = 3;
				p2.death_time = 0;
			}

			if (p1.mode == 2 && p2.mode == 1) {
				uint16_t get_score = p1.score - p1.score / 2;
				p1.score -= get_score;
				p2.score += get_score;
				p1.mode = 3;
				p1.death_time = 0;
			}

			// glm::vec2 dir = p12 / std::sqrt(len2);
			// //mirror velocity to be in separating direction:
			// glm::vec2 v12 = p2.velocity - p1.velocity;
			// glm::vec2 delta_v12 = dir * glm::max(0.0f, -1.75f * glm::dot(dir, v12));
			// p2.velocity += 0.5f * delta_v12;
			// p1.velocity -= 0.5f * delta_v12;
		}
		//player/arena collisions:
		if (p1.position.x < ArenaMin.x + PlayerRadius) {
			p1.position.x = ArenaMin.x + PlayerRadius;
			// p1.velocity.x = std::abs(p1.velocity.x);
		}
		if (p1.position.x > ArenaMax.x - PlayerRadius) {
			p1.position.x = ArenaMax.x - PlayerRadius;
			// p1.velocity.x =-std::abs(p1.velocity.x);
		}
		if (p1.position.y < ArenaMin.y + PlayerRadius) {
			p1.position.y = ArenaMin.y + PlayerRadius;
			// p1.velocity.y = std::abs(p1.velocity.y);
		}
		if (p1.position.y > ArenaMax.y - PlayerRadius) {
			p1.position.y = ArenaMax.y - PlayerRadius;
			// p1.velocity.y =-std::abs(p1.velocity.y);
		}
		//player/block collisions:
		for (auto const &block :MAP.blocks) {
			if (p1.position.x > block.left_down_corner.x - PlayerRadius && p1.position.x < block.left_down_corner.x + block_size + PlayerRadius
			    && p1.position.y > block.left_down_corner.y - PlayerRadius && p1.position.y < block.left_down_corner.y + block_size + PlayerRadius) {
				if (p1.velocity.x > 0) {
					p1.position.x = block.left_down_corner.x - PlayerRadius;
				}
				if (p1.velocity.x < 0) {
					p1.position.x = block.left_down_corner.x + block_size + PlayerRadius;
				}
				if (p1.velocity.y > 0) {
					p1.position.y = block.left_down_corner.y - PlayerRadius;
				}
				if (p1.velocity.y < 0) {
					p1.position.y = block.left_down_corner.y + block_size + PlayerRadius;
				}
				p1.velocity.x = 0;
				p1.velocity.y = 0;
			}
		}

		for (auto &consumable :consumables) {
			glm::vec2 p1c = p1.position - consumable.center;
			float len2 = glm::length2(p1c);
			float touch_dist = consumable_size + PlayerRadius;
			uint16_t cons_score = 1;
			if (consumable.size == Consumable::big) {
				// big consumable
				touch_dist = 2 * consumable_size + PlayerRadius;
				cons_score = 2;
			} 
			if (p1.mode == 2 && consumable.consumed == false && len2 < touch_dist * touch_dist) {
				p1.score += cons_score;
				consumable.consumed = true;
			}
		}
	}
}


void Game::send_state_message(Connection *connection_, Player *connection_player) const {
	assert(connection_);
	auto &connection = *connection_;

	connection.send(Message::S2C_State);
	//will patch message size in later, for now placeholder bytes:
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	connection.send(uint8_t(0));
	size_t mark = connection.send_buffer.size(); //keep track of this position in the buffer


	//send player info helper:
	auto send_player = [&](Player const &player) {
		connection.send(player.position);
		connection.send(player.velocity);
		connection.send(player.color);
		connection.send(player.score);
		connection.send(player.mode);
		connection.send(player.speed_sp);
	
		//NOTE: can't just 'send(name)' because player.name is not plain-old-data type.
		//effectively: truncates player name to 255 chars
		uint8_t len = uint8_t(std::min< size_t >(255, player.name.size()));
		connection.send(len);
		connection.send_buffer.insert(connection.send_buffer.end(), player.name.begin(), player.name.begin() + len);
	};

	//player count:
	connection.send(uint8_t(players.size()));
	if (connection_player) send_player(*connection_player);
	for (auto const &player : players) {
		if (&player == connection_player) continue;
		send_player(player);
	}

	auto send_consumable = [&](Consumable const &consumable) {
		connection.send(consumable.center);
		connection.send(consumable.size);
		connection.send(consumable.color);
		connection.send(uint8_t(consumable.consumed));
	};

	connection.send(uint8_t(consumables.size()));
	for (auto const &consumable : consumables) {
		send_consumable(consumable);
	}

	connection.send(mode);
	connection.send(ready_seconds);
	connection.send(playing_seconds);

	connection.send(predator_name);

	//compute the message size and patch into the message header:
	uint32_t size = uint32_t(connection.send_buffer.size() - mark);
	connection.send_buffer[mark-3] = uint8_t(size);
	connection.send_buffer[mark-2] = uint8_t(size >> 8);
	connection.send_buffer[mark-1] = uint8_t(size >> 16);
}

bool Game::recv_state_message(Connection *connection_) {
	assert(connection_);
	auto &connection = *connection_;
	auto &recv_buffer = connection.recv_buffer;

	if (recv_buffer.size() < 4) return false;
	if (recv_buffer[0] != uint8_t(Message::S2C_State)) return false;
	uint32_t size = (uint32_t(recv_buffer[3]) << 16)
	              | (uint32_t(recv_buffer[2]) << 8)
	              |  uint32_t(recv_buffer[1]);
	uint32_t at = 0;
	//expecting complete message:
	if (recv_buffer.size() < 4 + size) return false;

	//copy bytes from buffer and advance position:
	auto read = [&](auto *val) {
		if (at + sizeof(*val) > size) {
			throw std::runtime_error("Ran out of bytes reading state message.");
		}
		std::memcpy(val, &recv_buffer[4 + at], sizeof(*val));
		at += sizeof(*val);
	};

	players.clear();
	uint8_t player_count;
	read(&player_count);
	for (uint8_t i = 0; i < player_count; ++i) {
		players.emplace_back();
		Player &player = players.back();
		read(&player.position);
		read(&player.velocity);
		read(&player.color);
		read(&player.score);
		read(&player.mode);
		read(&player.speed_sp);
		uint8_t name_len;
		read(&name_len);
		//n.b. would probably be more efficient to directly copy from recv_buffer, but I think this is clearer:
		player.name = "";
		for (uint8_t n = 0; n < name_len; ++n) {
			char c;
			read(&c);
			player.name += c;
		}
	}

	consumables.clear();
	uint8_t consumable_count;
	read(&consumable_count);
	for (uint8_t i = 0; i < consumable_count; ++i) {
		consumables.emplace_back();
		Consumable &consumable = consumables.back();
		read(&consumable.center);
		read(&consumable.size);
		read(&consumable.color);
		read(&consumable.consumed);
	}

	read(&mode);
	read(&ready_seconds);
	read(&playing_seconds);

	read(&predator_name);

	if (at != size) throw std::runtime_error("Trailing data in state message.");

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}

void Game::load_map(){
	std::ifstream infile(data_path("map.txt"));
	std::string line;
	std::string cur_text;


	int top = 0;	
	while (std::getline(infile, line)){
		//assert(false);
		//assert(line.length() == 16);
		map_line_length = (int)line.length();
		block_size = 2.0f / map_line_length;
		consumable_size = 1.0f / map_line_length / 2;
		top++;
		for (int i = 0; i < map_line_length; i++){
			float x = -1.0f + i * block_size;
			float y = 1.0f - top * block_size;
			if (line[i] == 'X'){
				glm::vec2 pos = glm::vec2(x,y);
				MAP.blocks.push_back(Block{pos});
				//assert(false);
				//std::cout << line[i];
			}else if(line[i] == 'o'){
				consumables.push_back(Consumable{glm::vec2(x + block_size/2,y+block_size/2), Consumable::big,glm::u8vec4(rand() % 255, rand() % 255, rand() % 255, 0xff),false});
			}else if(line[i] == '1'){
				player1_spawn = glm::vec2(x + block_size/2,y+block_size/2);
			}else if(line[i] == '2'){
				player2_spawn = glm::vec2(x + block_size/2,y+block_size/2);
			}else if(line[i] == '.'){
				consumables.push_back(Consumable{glm::vec2(x + block_size/2,y+block_size/2), Consumable::small,glm::u8vec4(rand() % 255, rand() % 255, rand() % 255, 0xff),false});
			}
		}

	}
	//assert(false);
}
