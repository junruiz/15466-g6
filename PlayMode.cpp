#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>


//Basic that meets game requirement 
// Only 2 person, raise error if third join Chen
//1. Loading text into map  Chen 
//2. Scoreboard two players updating Chen
//3. Player mode-switching predator vs prey Chen
//4. Different player should appear differently Chen
//5. Player-player collision Zhao
//6. Time (猎杀时间) Zhao
//7. Player-object collision consumption Zhao 
//8. Spawn consumable (After I've down map)
//9. different consumable should appear differently Note: diffirent color solid circle Zhao 


//Extra feature:
//7.Skill list: 蓄力，加速, 减速弹，眩晕弹
//8.Different music for Different player



PlayMode::PlayMode(Client &client_) : client(client_) {
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.repeat) {
			//ignore repeats
		} else if (evt.key.keysym.sym == SDLK_a) {
			controls.left.downs += 1;
			controls.left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			controls.right.downs += 1;
			controls.right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			controls.up.downs += 1;
			controls.up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			controls.down.downs += 1;
			controls.down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			controls.jump.downs += 1;
			controls.jump.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			controls.left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			controls.right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			controls.up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			controls.down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			controls.jump.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//queue data for sending to server:
	if (gameReady){
		controls.send_controls_message(&client.connection);
	}
	

	//reset button press counters:

	controls.left.downs = 0;
	controls.right.downs = 0;
	controls.up.downs = 0;
	controls.down.downs = 0;
	controls.jump.downs = 0;

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG
			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c)) handled_message = true;
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
				//quit the game:
				throw e;
			}
		}
	}, 0.0);

	if (game.players.size() >=2){
		gameReady = true;
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	static std::array< glm::vec2, 16 > const circle = [](){
		std::array< glm::vec2, 16 > ret;
		for (uint32_t a = 0; a < ret.size(); ++a) {
			float ang = a / float(ret.size()) * 2.0f * float(M_PI);
			ret[a] = glm::vec2(std::cos(ang), std::sin(ang));
		}
		return ret;
	}();

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	
	//figure out view transform to center the arena:
	float aspect = float(drawable_size.x) / float(drawable_size.y);
	float scale = std::min(
		2.0f * aspect / (Game::ArenaMax.x - Game::ArenaMin.x + 2.0f * Game::PlayerRadius),
		2.0f / (Game::ArenaMax.y - Game::ArenaMin.y + 2.0f * Game::PlayerRadius)
	);
	glm::vec2 offset = -0.5f * (Game::ArenaMax + Game::ArenaMin);

	glm::mat4 world_to_clip = glm::mat4(
		scale / aspect, 0.0f, 0.0f, offset.x,
		0.0f, scale, 0.0f, offset.y,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	{
		DrawLines lines(world_to_clip);

		//helper:
		auto draw_text = [&](glm::vec2 const &at, std::string const &text, float H) {
			lines.draw_text(text,
				glm::vec3(at.x, at.y, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = (1.0f / scale) / drawable_size.y;
			lines.draw_text(text,
				glm::vec3(at.x + ofs, at.y + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		};


		glm::vec2 scoreBoardMin = glm::vec2((float)Game::ArenaMax.x + 0.01f, (float)Game::ArenaMin.y);
		glm::vec2 scoreBoardMax = glm::vec2((float)Game::ArenaMax.x + 0.5f, (float)Game::ArenaMax.y);

		lines.draw(glm::vec3(scoreBoardMin.x, scoreBoardMin.y, 0.0f), glm::vec3(scoreBoardMax.x, scoreBoardMin.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
		lines.draw(glm::vec3(scoreBoardMin.x, scoreBoardMax.y, 0.0f), glm::vec3(scoreBoardMax.x, scoreBoardMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
		lines.draw(glm::vec3(scoreBoardMin.x, scoreBoardMin.y, 0.0f), glm::vec3(scoreBoardMin.x, scoreBoardMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
		lines.draw(glm::vec3(scoreBoardMax.x, scoreBoardMin.y, 0.0f), glm::vec3(scoreBoardMax.x, scoreBoardMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));

		//float moveX = -100.0f;
		lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMin.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
		lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMax.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
		lines.draw(glm::vec3(Game::ArenaMin.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMin.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));
		lines.draw(glm::vec3(Game::ArenaMax.x, Game::ArenaMin.y, 0.0f), glm::vec3(Game::ArenaMax.x, Game::ArenaMax.y, 0.0f), glm::u8vec4(0xff, 0x00, 0xff, 0xff));

		for (auto const &block: game.MAP.blocks) {
			float corner_xleft = block.left_down_corner.x;
			float corner_xright = block.left_down_corner.x + game.block_size;
			float corner_ybottom = block.left_down_corner.y;
			float corner_ytop = block.left_down_corner.y + game.block_size;
			lines.draw(glm::vec3(corner_xleft, corner_ybottom, 0.0f), glm::vec3(corner_xright, corner_ybottom, 0.0f), glm::u8vec4(0xff, 0xff, 0x00, 0xff));
			lines.draw(glm::vec3(corner_xleft, corner_ytop, 0.0f), glm::vec3(corner_xright, corner_ytop, 0.0f), glm::u8vec4(0xff, 0xff, 0x00, 0xff));
			lines.draw(glm::vec3(corner_xleft, corner_ybottom, 0.0f), glm::vec3(corner_xleft, corner_ytop, 0.0f), glm::u8vec4(0xff, 0xff, 0x00, 0xff));
			lines.draw(glm::vec3(corner_xright, corner_ybottom, 0.0f), glm::vec3(corner_xright, corner_ytop, 0.0f), glm::u8vec4(0xff, 0xff, 0x00, 0xff));
		}

		for (auto const &consumable: game.consumables) {
			float cons_size = game.consumable_size;
			glm::u8vec4 col = glm::u8vec4(0xff, 0xff, 0, 0xff);
			if (consumable.size == Consumable::big) {
				// big type
				cons_size *= 2;
				col = consumable.color;
			}

			// std::cout << consumable.consumed << "\n";
			if (!consumable.consumed) {
				for (uint32_t a = 0; a < circle.size(); ++a) {
					lines.draw(
						glm::vec3(consumable.center + cons_size * circle[a], 0.0f),
						glm::vec3(consumable.center + cons_size * circle[(a+1)%circle.size()], 0.0f),
						col
					);
				}
			}
		}

		for (auto const &player : game.players) {
			glm::u8vec4 col = glm::u8vec4(player.color.x*255, player.color.y*255, player.color.z*255, 0xff);
			if (&player == &game.players.front()) {
				//mark current player (which server sends first):
				lines.draw(
					glm::vec3(player.position + Game::PlayerRadius * glm::vec2(-0.5f,-0.5f), 0.0f),
					glm::vec3(player.position + Game::PlayerRadius * glm::vec2( 0.5f, 0.5f), 0.0f),
					col
				);
				lines.draw(
					glm::vec3(player.position + Game::PlayerRadius * glm::vec2(-0.5f, 0.5f), 0.0f),
					glm::vec3(player.position + Game::PlayerRadius * glm::vec2( 0.5f,-0.5f), 0.0f),
					col
				);
			}
			for (uint32_t a = 0; a < circle.size(); ++a) {
				lines.draw(
					glm::vec3(player.position + Game::PlayerRadius * circle[a], 0.0f),
					glm::vec3(player.position + Game::PlayerRadius * circle[(a+1)%circle.size()], 0.0f),
					col
				);
			}

			draw_text(player.position + glm::vec2(0.0f, -0.1f + Game::PlayerRadius), player.name, 0.09f);

			//drawing score draws at the topright corner for now
			std::string inputString = std::to_string(player.score);
			glm::vec2 scorePos = glm::vec2(scoreBoardMin.x + 0.01f,scoreBoardMax.y-0.2f);
			draw_text(scorePos, player.name +  ": "+ inputString, 0.09f);
		}
	}
	GL_ERRORS();
}


