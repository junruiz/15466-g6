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
	send_button(jump);
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
	recv_button(recv_buffer[4+4], &jump);

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
	player.position.x = glm::mix(ArenaMin.x + 2.0f * PlayerRadius, ArenaMax.x - 2.0f * PlayerRadius, 0.4f + 0.2f * mt() / float(mt.max()));
	player.position.y = glm::mix(ArenaMin.y + 2.0f * PlayerRadius, ArenaMax.y - 2.0f * PlayerRadius, 0.4f + 0.2f * mt() / float(mt.max()));

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
	//position/velocity update:
	for (auto &p : players) {
		glm::vec2 dir = glm::vec2(0.0f, 0.0f);
		if (p.controls.left.pressed) dir = glm::vec2(-1.0f, 0.0f);
		if (p.controls.right.pressed) dir = glm::vec2(1.0f, 0.0f);
		if (p.controls.down.pressed) dir = glm::vec2(0.0f, -1.0f);
		if (p.controls.up.pressed) dir = glm::vec2(0.0f, 1.0f);

		if (dir == glm::vec2(0.0f)) {
			//no inputs: just drift to a stop
			// float amt = 1.0f - std::pow(0.5f, elapsed / (PlayerAccelHalflife * 2.0f));
			// p.velocity = glm::mix(p.velocity, glm::vec2(0.0f,0.0f), amt);
			p.velocity = glm::vec2(0.0f, 0.0f);
		} else {
			//inputs: tween velocity to target direction

			dir = glm::normalize(dir);

			// float amt = 1.0f - std::pow(0.5f, elapsed / PlayerAccelHalflife);

			// //accelerate along velocity (if not fast enough):
			// float along = glm::dot(p.velocity, dir);
			// if (along < PlayerSpeed) {
			// 	along = glm::mix(along, PlayerSpeed, amt);
			// }

			// //damp perpendicular velocity:
			// float perp = glm::dot(p.velocity, glm::vec2(-dir.y, dir.x));
			// perp = glm::mix(perp, 0.0f, amt);

			// p.velocity = dir * along + glm::vec2(-dir.y, dir.x) * perp;
			p.velocity = dir * 1.0f;
		}
		p.position += p.velocity * elapsed;
		p.survived_time =1.0f;
		//reset 'downs' since controls have been handled:
		p.controls.left.downs = 0;
		p.controls.right.downs = 0;
		p.controls.up.downs = 0;
		p.controls.down.downs = 0;
		p.controls.jump.downs = 0;
	}

	//collision resolution:
	for (auto &p1 : players) {
		//player/player collisions:
		for (auto &p2 : players) {
			if (&p1 == &p2) break;
			// update this to be "eat"

			// glm::vec2 p12 = p2.position - p1.position;
			// float len2 = glm::length2(p12);
			// if (len2 > (2.0f * PlayerRadius) * (2.0f * PlayerRadius)) continue;
			// if (len2 == 0.0f) continue;
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

		for (auto &consumable : consumables) {
			glm::vec2 p1c = p1.position - consumable.center;
			float len2 = glm::length2(p1c);
			float touch_dist = consumable_size + PlayerRadius;
			uint16_t cons_score = 1;
			if (consumable.size == Consumable::big) {
				// big consumable
				touch_dist = 2 * consumable_size + PlayerRadius;
				cons_score = 2;
			} 
			if (consumable.consumed == false && len2 < touch_dist * touch_dist) {
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

	if (at != size) throw std::runtime_error("Trailing data in state message.");

	//delete message from buffer:
	recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + 4 + size);

	return true;
}

void Game::load_map(){
	std::ifstream infile(data_path("map.txt"));
	std::string line;
	std::string cur_text;

	
	int top = 16;
	while (std::getline(infile, line)){
		//assert(false);
		//assert(line.length() == 16);
		map_line_length = line.length();
		block_size = 2.0f / map_line_length;
		top--;
		for (int i = 0; i < map_line_length; i++){
			if (line[i] == 'X'){
				float x = -1.0f + i * block_size;
				float y = -1.0f + top * block_size;
				glm::vec2 pos = glm::vec2(x,y);
				MAP.blocks.push_back(Block{pos});
				//assert(false);
				//std::cout << line[i];
			}else{
				//std::cout << "Hello World!";
			}
		}

	}
	//assert(false);
}
