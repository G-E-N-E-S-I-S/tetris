#include "stdafx.h"
#include "tetris.hpp"

// ENTRYPOINT
int main()
{
	auto my_console = console_controller(GetStdHandle(STD_OUTPUT_HANDLE));
	auto tetris_game = tetris(my_console, 20, 15);

	tetris_game.run();

	return 0;
}

// MAIN
void tetris::run()
{
	// PREPARE CONSOLE WINDOW
	this->console.set_title(L"TETRIS");

	// DRAW GAME BORDER, THIS IS WILL NOT BE TOUCHED
	// DURING GAME PLAY
	this->draw_boundary();

	// TETROMINO LIST IS FORMATTED AS: POSITION, IS_MOVING, TETROMINO 
	// ADD FIRST RANDOM PIECE, AFTERWARDS ADD A PIECE EVERY TIME A PIECE 
	this->pieces.emplace_back(tetromino_data{ this->get_random_start_position(), true, this->get_random_tetromino() });

	// START GAME LOOP
	this->game_loop();
}

// DRAW BOUNDARIES
void tetris::draw_boundary()
{
	const auto border_character = L'█';
	for (int16_t i = 0; i <= this->border_height; i++)
		this->console.fill_horizontal(0, i, border_character, border_width);

	this->clear_game_frame();
}

// CLEAR INSIDE OF BORDER
void tetris::clear_game_frame()
{
	this->console.clear(1, 1, this->border_width - 2, this->border_height - 1);
}

// DRAW TETRIS PIECE, ALSO KNOWN AS TETROMINO, PART BY PART
void tetris::draw_tetromino(const screen_vector position, tetromino piece)
{
	auto[x, y] = position;

	for (size_t part_index = 0; part_index < piece.get_size(); part_index++)
	{
		const auto part = piece[part_index];
		this->console.draw(x + part.x, y + part.y, piece.get_character());
	}
}

tetromino tetris::get_random_tetromino()
{
	// https://en.wikipedia.org/wiki/Tetris#Tetromino_colors
	static std::array<tetromino, 6> pieces =
	{
		// I TETROMINO
		// #
		// #
		// #
		// #
		tetromino('#',{ { 0, 0 },{ 0, 1 },{ 0, 2 },{ 0, 3 } }),

		// J TETROMINO
		// &&&
		//   &
		tetromino('&',{ { 0, 0 },{ 1, 0 },{ 2, 0 },{ 2, 1 } }),

		// L TETROMINO
		// @@@
		// @
		tetromino('@',{ { 0, 0 },{ 1, 0 },{ 2, 0 },{ 0, 1 } }),

		// O TETROMINO
		// $$
		// $$
		tetromino('$',{ { 0, 0 },{ 1, 0 },{ 0, 1 },{ 1, 1 } }),


		// T TETROMINO
		// +++
		//  +
		tetromino('+',{ { 0, 0 },{ -1, 0 },{ 1, 0 },{ 0, 1 } }),

		// Z TETROMINO
		// %%
		//  %%
		tetromino('%',{ { 0, 0 },{ -1, 0 },{ 0, 1 },{ 1, 1 } }),
	};


	return pieces.at(rng::get_int<size_t>(0, 5));
}

screen_vector tetris::get_random_start_position()
{
	return screen_vector{ rng::get_int<int16_t>(4, this->border_width - 4), 1 };
}

void tetris::game_loop()
{
	auto update_move = std::chrono::steady_clock::now();
	while (true)
	{
		// TIME AT BEGINNING OF LOOP TO CAP FRAMERATE AT PRECISELY 30 FPS
		auto start_time = std::chrono::steady_clock::now();

		// CLEAR INSIDE OF FRAME EVERY TICK
		this->clear_game_frame();

		// ONLY MOVE CONTROLLABLE TETROMINO EVERY x ms
		auto time_delta = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - update_move);

		// RESET TIMER WHEN MOVING TETROMINO
		const auto move_piece = time_delta.count() > 250;
		if (move_piece)
			update_move = std::chrono::steady_clock::now();

		// SET TO TRUE WHEN READY TO ADD A NEW PIECE
		// ONLY DO SO WHEN MOVING PIECE STOPS
		auto add_new_piece = false;

		// ITERATE ALL PIECES IN GAME TO DRAW
		// ONE OF THEM WILL ALWAYS BE CONTROLLABLE AND MOVING
		for (auto& piece_data : this->pieces)
		{
			// DRAW TETROMINO AT CURRENT POSITION
			this->draw_tetromino(piece_data.position, piece_data.piece);

			// CHECK IF TETROMINO IS MOVING
			if (piece_data.is_moving)
			{
				// MOVE TETROMINO IF PLAYER
				this->handle_controls(piece_data);

				// MOVE TETROMINO DOWN ONCE EVERY x MS
				if (move_piece)
					this->move_piece(piece_data, add_new_piece);

				if (add_new_piece)
				{
					// LOCK MOVING PIECE IN PLACE
					this->add_solid_parts(piece_data.piece.get_elements(), piece_data.position);

					// ADD NEW PIECE IF MOVING PIECE WAS LOCKED IN PLACE
					this->pieces.emplace_back(this->get_random_start_position(), true, this->get_random_tetromino());
					add_new_piece = false;
				}
			}
		}

		// SLEEP UNTIL ~33ms HAS PASSED FOR CONSISTENT 30 FPS
		std::this_thread::sleep_until(start_time + std::chrono::milliseconds(1000 / 30));
	}
}

void tetris::add_solid_parts(part_vector_t& parts, screen_vector& position)
{
	for (auto part : parts)
	{
		auto new_absolute_position = screen_vector{ position.x + part.x, position.y + part.y };
		solid_parts.emplace_back(new_absolute_position);
	}
}

void tetris::handle_controls(tetromino_data& data)
{
	auto position_copy = data.position;

	// MOVE RIGHT
	if (this->console.get_key_press(VK_RIGHT))
	{
		++position_copy.x;
		if (!this->does_element_collide(data.piece, position_copy))
			++data.position.x;
	}

	// MOVE LEFT
	if (this->console.get_key_press(VK_LEFT))
	{
		--position_copy.x;
		if (!this->does_element_collide(data.piece, position_copy))
			--data.position.x;
	}

	// MOVE DOWN
	if (this->console.get_key_press(VK_DOWN))
	{
		++position_copy.y;
		if (!this->does_element_collide(data.piece, position_copy))
			++data.position.y;
	}

	// ROTATE 90 DEGREES CLOCKWISE
	if (this->console.get_key_press(VK_UP))
	{
		auto new_piece = data.piece.rotate(90.f);

		if (!this->does_element_collide(new_piece, data.position))
			data.piece = new_piece;
	}
}

void tetris::move_piece(tetromino_data& data, bool& add_new_piece)
{
	// MOVE TETROMINO DOWN ONCE TO CHECK FOR COLLISION
	auto copy_position = data.position;
	++copy_position.y;

	// IF ANY FUTURE BLOCK COLLIDES, LOCK TETROMINO IN PLACE
	if (!this->does_element_collide(data.piece, copy_position))
	{
		++data.position.y;
	}
	else
	{
		// LOCK TETROMINO IN PLACE
		data.is_moving = false;
		add_new_piece = true;
	}
}

bool tetris::does_element_collide(tetromino& piece, screen_vector position)
{
	auto& parts = piece.get_elements();
	for (auto part : parts)
	{
		// COLLISION! LOCK TETROMINO IN PLACE AND SPAWN A NEW TETROMINO
		if (this->collides(part, position))
			return true;
	}

	return false;
}

bool tetris::collides(screen_vector part, screen_vector position)
{
	auto absolute_position = screen_vector{ position.x + part.x, position.y + part.y };

	// COLLIDED WITH ANOTHER SOLID TETROMINO?
	auto& solid_parts = this->solid_parts;
	auto piece_collision = std::find(solid_parts.begin(), solid_parts.end(), absolute_position) != solid_parts.end();

	// COLLIDED WITH BORDER?
	auto border_collision =
		absolute_position.y >= this->border_height ||		// COLLIDING WITH BOTTOM BORDER
		absolute_position.x < 1 ||							// COLLIDING WITH LEFT BORDER
		absolute_position.x > this->border_width - 2 ||		// COLLIDING WITH RIGHT BORDER
		absolute_position.y < 2;							// COLLIDING WITH TOP BORDER

	// RETURN TRUE EITHER WAY, ANY COLLISION SHALL HALT MOVEMENT
	return piece_collision || border_collision;
}
