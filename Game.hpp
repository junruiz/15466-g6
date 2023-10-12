#pragma once

#include <glm/glm.hpp>

#include <string>
#include <list>
#include <random>

struct Connection;

//Game state, separate from rendering.

//Currently set up for a "client sends controls" / "server sends whole state" situation.

enum class Message : uint8_t {
	C2S_Controls = 1, //Greg!
	S2C_State = 's',
	//...
};

//used to represent a control input:
struct Button {
	uint8_t downs = 0; //times the button has been pressed
	bool pressed = false; //is the button pressed now
};

//state of one player in the game:
struct Player {
	//player inputs (sent from client):
	struct Controls {
		Button left, right, up, down, jump;

		void send_controls_message(Connection *connection) const;

		//returns 'false' if no message or not a controls message,
		//returns 'true' if read a controls message,
		//throws on malformed controls message
		bool recv_controls_message(Connection *connection);
	} controls;

	//player state (sent from server):
	glm::vec2 position = glm::vec2(0.0f, 0.0f);
	glm::vec2 velocity = glm::vec2(0.0f, 0.0f);

	glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
	std::string name = "";

	//Not Ready 0 cannot touch consumable, cannot eat others/be eaten
	//Predator 1 cannot touch consumable, can eat others
	//Prey 2 can touch consumable, can be eaten
	//Invincible 3 cannot touch consumable, cannot eat others/be eaten

	int mode = 0;
	float death_time = 0;
	//Added player attribution
	uint16_t score = 0;
};

struct Block {
	glm::vec2 left_down_corner = glm::vec2(0.0f, 0.0f);
};

struct map{
	std::list< Block > blocks = {};
};
struct Consumable {
	glm::vec2 center = glm::vec2(0, 0);
	enum type {big, small} size = small;
	glm::u8vec4 color = glm::u8vec4(rand() % 255, rand() % 255, rand() % 255, 0xff);
	bool consumed = false;
};

struct Game {
	std::list< Player > players; //(using list so they can have stable addresses)
	Player *spawn_player(); //add player the end of the players list (may also, e.g., play some spawn anim)
	void remove_player(Player *); //remove player from game (may also, e.g., play some despawn anim)

	std::mt19937 mt; //used for spawning players
	uint32_t next_player_number = 1; //used for naming players

	Game();

	//state update function:
	void update(float elapsed);

	//constants:
	//the update rate on the server:
	inline static constexpr float Tick = 1.0f / 30.0f;

	//arena size:
	inline static constexpr glm::vec2 ArenaMin = glm::vec2(-1.0f, -1.0f);
	inline static constexpr glm::vec2 ArenaMax = glm::vec2( 1.0f,  1.0f);

	//game mode
	//mode 0: waiting
	//mode 1: preparing
	//mode 2: playing
	//mode 3: end
	int mode = 0;

	//time:
	float time = 0;
	uint8_t ready_seconds = 0;
	uint8_t playing_seconds = 0;
	float change_predator_time = 0;

	//set predator:
	int predator = 1;
	std::string predator_name = "Player 1";

	//Consumables:
	std::list< Consumable > consumables = {};

	//blockes:
	std::list< Block > blocks = {};
	map MAP = {};
	
	//default length 16
	int map_line_length = 16;
	float block_size = 2.0f / map_line_length;
	float consumable_size = 1.0f / map_line_length / 2;

	glm::vec2 player1_spawn = glm::vec2(0.0f,0.0f);
	glm::vec2 player2_spawn = glm::vec2(0.0f,0.0f);


	//player constants:
	inline static constexpr float PlayerRadius = 0.04f;
	inline static constexpr float PlayerSpeed = 2.0f;
	inline static constexpr float PlayerAccelHalflife = 0.25f;
	

	//---- communication helpers ----

	//used by client:
	//set game state from data in connection buffer
	// (return true if data was read)
	bool recv_state_message(Connection *connection);

	//used by server:
	//send game state.
	//  Will move "connection_player" to the front of the front of the sent list.
	void send_state_message(Connection *connection, Player *connection_player = nullptr) const;

	void load_map();
};
