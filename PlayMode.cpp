#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"
#include "load_save_png.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>



// 1. 恶魔的武器，闪烁的身躯 我来吧
// 2. 减速 15cd +5 spawn consuamble 你来吧
// 3. 咋哇路都 30cd 对面控1s 我来吧
// 4. 加速/贤者时间 +3s -2s  15s 你来吧
// 5. 游戏结束 read me togther 




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
	controls.send_controls_message(&client.connection);

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
	float sum = 0.0f;
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
		
		auto draw_fork = [&](glm::vec2 at, float len,float width) {
			lines.draw(glm::vec3(at.x, at.y, 0.0f), glm::vec3(at.x, at.y + len, 0.0f), glm::u8vec4(0xff, 0x00, 0x00, 0xff));
			lines.draw(glm::vec3(at.x-width, at.y+ 0.66f*len, 0.0f), glm::vec3(at.x+width, at.y+0.66f*len, 0.0f), glm::u8vec4(0xff, 0x00, 0x00, 0xff));
			lines.draw(glm::vec3(at.x-width, at.y+ 0.66f*len, 0.0f), glm::vec3(at.x-width, (at.y+0.83f*len), 0.0f), glm::u8vec4(0xff, 0x00, 0x00, 0xff));
			lines.draw(glm::vec3(at.x+width, at.y+ 0.66f*len, 0.0f), glm::vec3(at.x+width, at.y+0.83f*len, 0.0f),glm::u8vec4(0xff, 0x00, 0x00, 0xff));
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

		float idx = 0;
		std::string p1name = "";
		std::string p2name = "";
		int p1score = 0;
		int p2score = 0;
		
		for (auto const &player : game.players) {
			idx ++;
			if (idx == 1) {
				p1score = player.score;
				p1name = player.name;
			} else {
				p2score = player.score;
				p2name = player.name;
			}
			glm::u8vec4 col = glm::u8vec4(player.color.x*255, player.color.y*255, player.color.z*255, 0xff);
			if (player.mode == 0) {
				col = glm::u8vec4(rand()%255, rand()%255, rand()%255, 0xff);
			}
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

			if (player.mode == 1){
				draw_fork(player.position + glm::vec2(0.05f,-0.05f),0.1f,0.1f/5);
			}

			//drawing score draws at the topright corner for now
			std::string inputString = std::to_string(player.score);
			glm::vec2 scorePos = glm::vec2(scoreBoardMin.x + 0.01f,scoreBoardMax.y-sum-0.2f-0.2f*idx);
			draw_text(scorePos, player.name +  ": "+ inputString, 0.09f);

			glm::vec2 predatorPos = glm::vec2(scoreBoardMin.x + 0.01f,scoreBoardMax.y-sum-1.0f);
			if (player.mode == 1) {
				draw_text(predatorPos, "Now the Predator is " + player.name, 0.05f);
			}

		}
		std::string timeString = "";
		std::string winnerString = p1score > p2score ? "Game End! Winner is " + p1name : 
				 	   			   p2score > p1score ? "Game End! Winner is " + p2name : "Draw";
		
		glm::vec2 timePos = glm::vec2(scoreBoardMin.x + 0.01f,scoreBoardMax.y-sum-0.2f);
		if (game.mode == 0) {
			timeString = "Waiting for another player";
		}
 		if (game.mode == 1) {
			timeString = "Game will start in " + std::to_string(10 - game.ready_seconds) + "s";
 		}
 		if (game.mode == 2) {
			timeString = "Countdown: " + std::to_string(60 - game.playing_seconds) + "s";
 		}
		if (game.mode == 3) {
			timeString = winnerString;
		}
 		draw_text(timePos, timeString, 0.05f);
		
	}
	GL_ERRORS();
}


