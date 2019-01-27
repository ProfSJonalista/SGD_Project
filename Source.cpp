#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <array>
#include <random>

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

auto ErrThrow = [](const std::string &e) {
	std::string errstr = e + " : " + SDL_GetError();
	SDL_Quit();
	throw std::runtime_error(errstr);
};

std::shared_ptr<SDL_Window> InitWindows(const int width = 320, const int height = 200) {
	if (SDL_Init(SDL_INIT_VIDEO) != 0) ErrThrow("SDL_InitVideo");

	SDL_Window *win = SDL_CreateWindow("FlappySGD",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		width, height, SDL_WINDOW_SHOWN);
	if (win == nullptr) ErrThrow("SDL_CreateWindow");
	std::shared_ptr<SDL_Window> window(win, [](SDL_Window * ptr) {
		SDL_DestroyWindow(ptr);
	});
	return window;
}

std::shared_ptr<SDL_Renderer> InitRenderer(std::shared_ptr<SDL_Window> window) {
	SDL_Renderer *ren = SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (ren == nullptr) ErrThrow("SDL_CreateRenderer");
	std::shared_ptr<SDL_Renderer> renderer(ren, [](SDL_Renderer * ptr) {
		SDL_DestroyRenderer(ptr);
	});
	return renderer;
}

std::shared_ptr<SDL_Texture> LoadTexture(const std::shared_ptr<SDL_Renderer> renderer, const std::string fname) {
	SDL_Surface *bmp = IMG_Load(fname.c_str());
	if (bmp == nullptr) ErrThrow("IMG_Load");
	std::shared_ptr<SDL_Surface> bitmap(bmp, [](SDL_Surface * ptr) {
		SDL_FreeSurface(ptr);
	});

	SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer.get(), bitmap.get());
	if (tex == nullptr) ErrThrow("SDL_CreateTextureFromSurface");
	std::shared_ptr<SDL_Texture> texture(tex, [](SDL_Texture * ptr) {
		SDL_DestroyTexture(ptr);
	});
	return texture;
}

std::shared_ptr<SDL_Texture> ShowText(const std::shared_ptr<SDL_Renderer> renderer, const char* textureText, const int size, const SDL_Color color) {
	if (TTF_Init() < 0) ErrThrow("TTF_Init");
	TTF_Font* Sans = TTF_OpenFont("Starjedi.ttf", size);
	SDL_Surface* surfaceMessage = TTF_RenderText_Solid(Sans, textureText, color);
	if (surfaceMessage == nullptr) ErrThrow("Font");
	std::shared_ptr<SDL_Surface> bitmap(surfaceMessage, [](SDL_Surface * ptr) {
		SDL_FreeSurface(ptr);
	});
	SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer.get(), bitmap.get());
	if (tex == nullptr) ErrThrow("SDL_CreateTextureFromSurfaceFont");
	std::shared_ptr<SDL_Texture> texture(tex, [](SDL_Texture * ptr) {
		SDL_DestroyTexture(ptr);
	});
	return texture;
}

using pos_t = std::array<double, 2>;

pos_t operator +(const pos_t &a, const pos_t &b) {
	return { a[0] + b[0], a[1] + b[1] };
}
pos_t operator -(const pos_t &a, const pos_t &b) {
	return { a[0] - b[0], a[1] - b[1] };
}
pos_t operator *(const pos_t &a, const pos_t &b) {
	return { a[0] * b[0], a[1] * b[1] };
}
pos_t operator *(const pos_t &a, const double &b) {
	return { a[0] * b, a[1] * b };
}

class GameObject {
public:
	pos_t Position;
	pos_t Velocity = { 0,0 };
	pos_t Size;
	SDL_Rect DestRect;
};

bool CheckCollision(SDL_Rect a, SDL_Rect b)
{
	//The sides of the rectangles
	int leftA, leftB;
	int rightA, rightB;
	int topA, topB;
	int bottomA, bottomB;

	//Calculate the sides of rect A
	leftA = a.x;
	rightA = a.x + a.w;
	topA = a.y;
	bottomA = a.y + a.h;

	//Calculate the sides of rect B
	leftB = b.x;
	rightB = b.x + b.w;
	topB = b.y;
	bottomB = b.y + b.h;

	//If any of the sides from A are outside of B
	if (bottomA <= topB)
	{
		return false;
	}

	if (topA >= bottomB)
	{
		return false;
	}

	if (rightA <= leftB)
	{
		return false;
	}

	if (leftA >= rightB)
	{
		return false;
	}

	//If none of the sides from A are outside B
	return true;
}

GameObject GetWall(pos_t Position, pos_t Velocity, pos_t Size) {
	GameObject Wall;
	Wall.Position = Position;
	Wall.Velocity = Velocity;
	Wall.Size = Size;

	return Wall;
}

int main(int argc, char **argv) {
	auto Window = InitWindows(SCREEN_WIDTH, SCREEN_HEIGHT);
	auto Renderer = InitRenderer(Window);

	auto BgTex = LoadTexture(Renderer, "bg.png");
	auto BirdyTex = LoadTexture(Renderer, "ptok.png");
	auto PipeTex = LoadTexture(Renderer, "pipe.png");

	SDL_Color LoseTextColor = { 255, 0, 0 };
	auto LoseText = ShowText(Renderer, "You lose!", 50, LoseTextColor);

	GameObject Player;
	Player.Size = { 32,32 };
	Player.Position[0] = SCREEN_WIDTH / 4;
	Player.Position[1] = SCREEN_HEIGHT / 2;

	double DeltaTime = 1 / 60.0; //Frames
	int Score = 0;
	float GameTime = 0;
	float Interval = 1;
	std::vector<GameObject> Walls;
	std::default_random_engine Generator;
	std::uniform_int_distribution<int> RandomInterval(3, 6);
	std::uniform_int_distribution<int> RandomPos(-150, 150);
	bool GameLost = false;

	int FrameTime = 0;
	int TextureWidth;
	int TextureHeight;
	int FrameWidth;
	int FrameHeight;

	SDL_QueryTexture(BirdyTex.get(), NULL, NULL, &TextureWidth, &TextureHeight);
	FrameWidth = TextureWidth / 4;
	FrameHeight = TextureHeight;
	SDL_Rect anim = { 0, 0, FrameWidth, FrameHeight };

	for (bool game_active = true; game_active; ) {
		GameTime += DeltaTime;
		if (GameTime > Interval) {
			Interval += RandomInterval(Generator);
			int pos = RandomPos(Generator);
			
			Walls.push_back(GetWall({ 832, (double)650 + pos }, { -1, 0 }, { 64, 512 }));
			Walls.push_back(GetWall({ 832, (double)-50 + pos }, { -1, 0 }, { 64, 512 }));
		}
		
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) { game_active = false; }
		}

		const Uint8 *KeyboardState = SDL_GetKeyboardState(NULL);

		if (!GameLost) {
			if (KeyboardState[SDL_SCANCODE_UP]) {
				Player.Position[1]--;
				Player.Velocity[1] = -8;
			}
		}

		Player.Position = Player.Position + Player.Velocity;
		Player.Velocity = Player.Velocity + pos_t{ 0, 0.5 };

		//Checking if player has reached window borderlines, if so, game is lost
		if ((Player.Position[1] < -Player.Size[1] / 2) || (Player.Position[1] > SCREEN_HEIGHT + Player.Size[1] / 2))
			GameLost = true;

		for (GameObject &wall : Walls)
		{
			wall.Position = wall.Position + wall.Velocity;
			if (CheckCollision(wall.DestRect, Player.DestRect)) {
				GameLost = true;
			}
		}

		SDL_RenderClear(Renderer.get());
		SDL_RenderCopy(Renderer.get(), BgTex.get(), NULL, NULL);

		for (GameObject &wall : Walls)
		{
			wall.DestRect = { (int)(wall.Position[0] - wall.Size[0] / 2), (int)(wall.Position[1] - wall.Size[1] / 2), (int)wall.Size[0], (int)wall.Size[1] };
			SDL_RenderCopy(Renderer.get(), PipeTex.get(), NULL, &wall.DestRect);
		}

		FrameTime++;
		if (FrameTime == 5) {
			FrameTime = 0;
			anim.x += FrameWidth;
			if (anim.x >= TextureWidth)
				anim.x = 0;
		}

		Player.DestRect = { (int)(Player.Position[0] - Player.Size[0] / 2), (int)(Player.Position[1] - Player.Size[1] / 2), (int)Player.Size[0], (int)Player.Size[1] };
		SDL_RenderCopy(Renderer.get(), BirdyTex.get(), &anim, &Player.DestRect);

		if (GameLost) {
			int texW = 0;
			int texH = 0;
			SDL_QueryTexture(LoseText.get(), NULL, NULL, &texW, &texH);
			SDL_Rect Message_rect = { 400 - texW / 2, 300 - texH / 2, texW, texH };
			SDL_RenderCopy(Renderer.get(), LoseText.get(), NULL, &Message_rect);
		}

		SDL_RenderPresent(Renderer.get());
		SDL_Delay((int)(DeltaTime / 1000));
	}
	return 0;
}