#include <iostream>
#include <fstream>
#include <SFML/Graphics.hpp>
#include <chrono>
#include <random>
#include <algorithm>
#include <sstream>
#include "TextureManager.h"

void placeMines(vector<vector<bool>>& mineLocations, int numMines, int numRows, int numCols) {
    std::default_random_engine generator(time(0)); // Seed the random number generator
    std::uniform_int_distribution<int> rowDistribution(0, numRows - 3); // Exclude last two rows
    std::uniform_int_distribution<int> colDistribution(0, numCols - 1);

    int minesPlaced = 0;
    while (minesPlaced < numMines) {
        int row = rowDistribution(generator);
        int col = colDistribution(generator);

        if (!mineLocations[row][col]) {
            mineLocations[row][col] = true;
            minesPlaced++;
        }
    }
}

bool allNonMineTilesRevealed(const std::vector<std::vector<sf::Sprite>>& tiles, const std::vector<std::vector<bool>>& mineLocations, const sf::Texture& tileHiddenTexture) {
    for (size_t i = 0; i < tiles.size(); ++i) {
        for (size_t j = 0; j < tiles[i].size(); ++j) {
            if (!mineLocations[i][j] && tiles[i][j].getTexture() == &tileHiddenTexture) {
                return false; // There's an unrevealed non-mine tile
            }
        }
    }
    return true; // All non-mine tiles are revealed
}

map<int, sf::Sprite> parseDigits(sf::Sprite digits){
    map<int, sf::Sprite> digitsMap;

    for(int i = 0; i < 10; i++){
        sf::IntRect rect(i*21,0,21,32);
        digits.setTextureRect(rect);
        sf::Sprite sprite = digits;
        digitsMap.emplace(i, sprite);
    }

    sf::IntRect negRect(10*21, 0, 21, 32);
    digits.setTextureRect(negRect);
    sf::Sprite negSign = digits;
    digitsMap.emplace(-1, negSign);

    return digitsMap;
}

void drawCount(sf::RenderWindow& window, int countFlags, int numRows, sf::Sprite digits) {
    std::map<int, sf::Sprite> digitsMap = parseDigits(digits);

    float posX = 33.0f;
    float posY = 32.0f * (numRows + 0.5f) + 16.0f;

    bool isNegative = countFlags < 0;
    std::string countString = std::to_string(std::abs(countFlags));

    if (isNegative) {
        sf::Sprite minusSign = digitsMap[-1];
        minusSign.setPosition(12.0f, posY);
        window.draw(minusSign);
        posX += 21.0f;
    }

    for (char digit : countString) {
        int digitIndex = digit - '0';
        sf::Sprite digitSprite = digitsMap[digitIndex];
        digitSprite.setPosition(posX, posY);
        window.draw(digitSprite);
        posX += 21.0f;
    }
}

void revealAdjacentTiles(std::vector<std::vector<sf::Sprite>>& tiles, std::vector<std::vector<bool>>& mineLocations, int row, int col, int numRows, int numCols, sf::Texture& hiddenTexture, sf::Texture& revealedTexture) {
    if (row < 0 || col < 0 || row >= numRows - 2 || col >= numCols) {
        return; // Out of bounds
    } 

    if (tiles[row][col].getTexture() != &hiddenTexture) {
        return; // Already revealed or flagged
    }

    int nearbyMines = 0;
    
    // Check neighboring tiles for mines
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            int newRow = row + i;
            int newCol = col + j;
            if (newRow >= 0 && newCol >= 0 && newRow < numRows - 2 && newCol < numCols && mineLocations[newRow][newCol]) {
                ++nearbyMines;
            }
        }
    }

    if (nearbyMines > 0) {
        std::string numberTexturePath = "number_" + std::to_string(nearbyMines);
        sf::Texture& numberTexture = TextureManager::getTexture(numberTexturePath);
        tiles[row][col].setTexture(numberTexture);
    } else {
        tiles[row][col].setTexture(revealedTexture);
        // Recursively reveal adjacent tiles
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                revealAdjacentTiles(tiles, mineLocations, row + i, col + j, numRows, numCols, hiddenTexture, revealedTexture);
            }
        }
    }
}

void updateLeaderboard(const std::string& playerName, float playerTime) {
    std::vector<std::pair<std::string, float>> leaderboardEntries;

    std::ifstream leaderboardFileIn("files/leaderboard.txt");
    if (leaderboardFileIn.is_open()) {
        std::string line;
        while (std::getline(leaderboardFileIn, line)) {
            std::istringstream iss(line);
            std::string name;
            float time;
            if (iss >> name >> time) {
                leaderboardEntries.push_back({name, time});
            }
        }
        leaderboardFileIn.close();
    }

    // Check if the player's name already exists in the leaderboard
    bool playerExists = false;
    for (auto& entry : leaderboardEntries) {
        if (entry.first == playerName) {
            playerExists = true;
            if (playerTime < entry.second) {
                entry.second = playerTime; // Update time if it's better
            }
            break;
        }
    }

    if (!playerExists) {
        leaderboardEntries.push_back({playerName, playerTime});
    }

    std::sort(leaderboardEntries.begin(), leaderboardEntries.end(),
              [](const std::pair<std::string, float>& a, const std::pair<std::string, float>& b) {
                  return a.second < b.second;
              });

    std::ofstream leaderboardFileOut("files/leaderboard.txt", std::ios::trunc);
    if (leaderboardFileOut.is_open()) {
        int count = 0;
        for (auto it = leaderboardEntries.begin(); it != leaderboardEntries.end() && count < 5; ++it) {
            leaderboardFileOut << it->first << " " << it->second << std::endl;
            ++count;
        }
        leaderboardFileOut.close();
    } else {
        std::cerr << "Unable to open leaderboard.txt for writing" << std::endl;
    }
}

void displayLeaderboardContent(sf::RenderWindow& leaderboardWindow, int numRows, int numCols) {
    std::ifstream leaderboardFile("files/leaderboard.txt");
    if (!leaderboardFile.is_open()) {
        std::cerr << "Unable to open leaderboard.txt" << std::endl;
        return;
    }

    std::vector<std::pair<std::string, float>> leaderboardEntries;

    // Read the file and parse leaderboard entries
    std::string line;
    while (std::getline(leaderboardFile, line)) {
        std::istringstream iss(line);
        std::string name;
        float time;
        if (iss >> name >> time) {
            leaderboardEntries.push_back({name, time});
        }
    }

    // Sort the leaderboard entries based on time
    std::sort(leaderboardEntries.begin(), leaderboardEntries.end(),
              [](const std::pair<std::string, float>& a, const std::pair<std::string, float>& b) {
                  return a.second < b.second;
              });

    sf::Font font;
    if (!font.loadFromFile("files/font.ttf")) {
        std::cerr << "Failed to load font.ttf" << std::endl;
        return;
    }

    sf::Text leaderboardText;
    leaderboardText.setFont(font);
    leaderboardText.setCharacterSize(20);
    leaderboardText.setFillColor(sf::Color::White);
    leaderboardText.setPosition((numCols*16)/2,((numRows*16)/2)-120);

    std::stringstream content;
    content << "\n";
    int count = 1;
    for (const auto& entry : leaderboardEntries) {
        content << count << ". " << entry.first << " - " << entry.second << "s\n";
        count++;
    }

    leaderboardText.setString(content.str());
    leaderboardText.setPosition(20.f, 40.f);

    leaderboardWindow.draw(leaderboardText);

}



int main() {

    std::ifstream txtFile("files/config.cfg");

    if (!txtFile.is_open()) {
        std::cout << "Unable to open file.\n";
        return 1;
    }

    int rowCount, colCount, numOfMines;

    txtFile >> colCount >> rowCount >> numOfMines;

    txtFile.close();


    sf::RenderWindow welcomeWindow(sf::VideoMode((colCount*32), (rowCount*32)+100), "Minesweeper");

    sf::Font font;
    if (!font.loadFromFile("files/font.ttf")) {
    std::cout << "Failed to load font.ttf" << std::endl;
    return 0;
    }

    
    sf::Text welcomeText;
    sf::Text welcomeText2;
    sf::Text userTypedName("",font, 18);
    welcomeText.setString("WELCOME TO MINESWEEPER!");
    welcomeText.setFont(font);
    welcomeText.setCharacterSize(24);
    welcomeText.setStyle(sf::Text::Bold | sf::Text::Underlined);
    welcomeText.setFillColor(sf::Color::White);

    sf::FloatRect welcomeTextRect = welcomeText.getLocalBounds();

    welcomeText.setOrigin(welcomeTextRect.left + welcomeTextRect.width/2.0f,
    welcomeTextRect.top + welcomeTextRect.height/2.0f);
    welcomeText.setPosition(sf::Vector2f((colCount*32)/2.0f, ((rowCount*32)+100)/2.0f - 150));

    std::string name;
    userTypedName.setFillColor(sf::Color::Yellow);
    

    sf::FloatRect userTypedNameRect = userTypedName.getLocalBounds();

    userTypedName.setOrigin(userTypedNameRect.left + userTypedNameRect.width/2.0f,
    userTypedNameRect.top + userTypedNameRect.height/2.0f);
    userTypedName.setPosition(sf::Vector2f((colCount*32)/2.0f, ((rowCount*32)+100)/2.0f - 45));


    sf::Text cursor;
    cursor.setFont(font);
    cursor.setCharacterSize(18);
    cursor.setFillColor(sf::Color::Yellow);
    cursor.setString("|");
    bool showCursor = true;

    welcomeText2.setString("Please enter your name:");
    welcomeText2.setFont(font);
    welcomeText2.setCharacterSize(20);
    welcomeText2.setStyle(sf::Text::Bold);
    welcomeText2.setFillColor(sf::Color::White);

    sf::FloatRect welcomeText2Rect = welcomeText2.getLocalBounds();

    welcomeText2.setOrigin(welcomeText2Rect.left + welcomeText2Rect.width/2.0f,
    welcomeText2Rect.top + welcomeText2Rect.height/2.0f);
    welcomeText2.setPosition(sf::Vector2f((colCount*32)/2.0f, ((rowCount*32)+100)/2.0f - 75));

    sf::RenderWindow gameWindow(sf::VideoMode(colCount*32, (rowCount*32)+100), "Minesweeper");

    while(welcomeWindow.isOpen()) {
        sf::Event event;
        while(welcomeWindow.pollEvent(event)) {
            if(event.type == sf::Event::Closed) {
            welcomeWindow.close();
            gameWindow.close();
            }
  

           if (event.type == sf::Event::TextEntered) {
                if (event.text.unicode < 128) {
                    char typed = static_cast<char>(event.text.unicode);

                    if (event.text.unicode == '\b') { // Handle backspace
                        if (!name.empty())
                            name.pop_back();
                    } else if (event.text.unicode != '\b' && name.size() < 10 && std::isalpha(typed)) {
                        if (name.empty() || (name.back() == ' ' && !isspace(typed))) {
                            typed = toupper(typed);
                        } else {
                            typed = tolower(typed);
                        }
                        name += typed;
                    }
                    userTypedName.setString(name);

                    sf::FloatRect textRect = userTypedName.getLocalBounds();
                    userTypedName.setOrigin(textRect.left + textRect.width / 2.0f,
                    textRect.top + textRect.height / 2.0f);
                    userTypedName.setPosition(sf::Vector2f((colCount*32)/2.0f, ((rowCount*32)+100)/2.0f - 45));
                }
            }
                if (event.key.code == sf::Keyboard::Enter) {
                    if (!name.empty()){
                        welcomeWindow.close();
                    } 
                }
            }
    
    welcomeWindow.clear(sf::Color::Blue);
    welcomeWindow.draw(welcomeText);
    welcomeWindow.draw(welcomeText2);
    if (showCursor) {
        cursor.setPosition(userTypedName.findCharacterPos(name.size()));
        welcomeWindow.draw(cursor);
    }
    welcomeWindow.draw(userTypedName);
    welcomeWindow.display();
    }

    //TIMER STUFF

    auto start_time = chrono::high_resolution_clock::now();

    auto pauseTime = chrono::high_resolution_clock::now();
    auto elapsed_paused_time = chrono::duration_cast<chrono::seconds>(chrono::high_resolution_clock::now() - pauseTime).count();

    bool paused = false; //false when game in not paused, true when the game is paused
    sf::Texture& digitsText = TextureManager::getTexture("digits");
    sf::Sprite digits;
    digits.setTexture(digitsText);

    map<int, sf::Sprite> digitsMap = parseDigits(digits); //map for my digits

    //GAME WINDOW STUFF

    sf::Texture& pauseText = TextureManager::getTexture("pause");
    sf::Sprite pauseBttn;
    pauseBttn.setTexture(pauseText);
    pauseBttn.setPosition((colCount*32)-240, 32*(rowCount+0.5f));

    sf::Texture& playText = TextureManager::getTexture("play");
    sf::Sprite playBttn;
    playBttn.setTexture(playText);
    playBttn.setPosition((colCount*32)-240, 32*(rowCount+0.5f));

    sf::Texture& leaderboardText = TextureManager::getTexture("leaderboard");
    sf::Sprite leaderboardBttn;
    leaderboardBttn.setTexture(leaderboardText);
    leaderboardBttn.setPosition((colCount*32) - 176, 32*(rowCount+0.5f));

    sf::Texture& debugText = TextureManager::getTexture("debug");
    sf::Sprite debugBttn;
    debugBttn.setTexture(debugText);
    debugBttn.setPosition((colCount*32) - 304, 32*(rowCount+0.5f));

    sf::Texture& happyFaceText = TextureManager::getTexture("face_happy");
    sf::Sprite happyFaceBttn;
    happyFaceBttn.setTexture(happyFaceText);
    happyFaceBttn.setPosition(((colCount/2.0f)*32) - 32, 32*(rowCount+0.5f));

    sf::Texture& faceWinText = TextureManager::getTexture("face_win");
    sf::Texture& faceLoseText = TextureManager::getTexture("face_lose");

    sf::Texture& tileHiddenText = TextureManager::getTexture("tile_hidden");
    sf::Texture& tileRevealedText = TextureManager::getTexture("tile_revealed");
    sf::Sprite tileHiddenBttn;
    tileHiddenBttn.setTexture(tileHiddenText);

    sf::Texture& mineText = TextureManager::getTexture("mine");

    sf::Texture& flagText = TextureManager::getTexture("flag");

    int cellWidth = (colCount*32)/colCount;
    int cellHeight = ((rowCount*32)+100)/rowCount; 


    float tileSizeX = static_cast<float>(tileHiddenText.getSize().x);
    float tileSizeY = static_cast<float>(tileHiddenText.getSize().y);


    vector<vector<sf::Sprite>> sprites;

    vector<vector<bool>> mineLocations(rowCount, vector<bool>(colCount, false));

    vector<vector<bool>> flaggedTiles(rowCount - 2, vector<bool>(colCount, false));

    int countFlags = numOfMines;

    placeMines(mineLocations, numOfMines, rowCount, colCount);

    for (int i = 0; i < rowCount; ++i) {

        std::vector<sf::Sprite> row;
        for (int j = 0; j < gameWindow.getSize().x / tileSizeX; ++j) {
            sf::Sprite sprite(tileHiddenText);
            sprite.setPosition(j * tileSizeX, i * tileSizeY);
            row.push_back(sprite);
        }
        sprites.push_back(row);
    }

    bool gameLost = false;
    bool gameWon = false;

    bool gameActive = true;

    bool debugMode = false;

    bool gameEnded = false;

    sf::FloatRect happyFaceBounds = happyFaceBttn.getGlobalBounds();

    

    while (gameWindow.isOpen()){
        sf::Event event;
        while(gameWindow.pollEvent(event)) {
            sf::Vector2i vec = sf::Mouse::getPosition(gameWindow);

            if(event.type == sf::Event::Closed) {
            gameWindow.close();
            }
            else if(event.type == sf::Event::MouseButtonPressed && gameActive){
                if (event.mouseButton.button == sf::Mouse::Left){
                    sf::Vector2i mousePos = sf::Mouse::getPosition(gameWindow);
                    int row = mousePos.y / tileSizeY;
                    int col = mousePos.x / tileSizeX;


                    if (leaderboardBttn.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                        paused = !paused;

                        //LEADERBOARD WINDOW
                        sf::RenderWindow leaderboardWindow(sf::VideoMode(colCount*16, (rowCount*16) + 50), "Minesweeper");

                        sf::Text leaderboardWelcome;
                        leaderboardWelcome.setString("LEADERBOARD");
                        leaderboardWelcome.setFont(font);
                        leaderboardWelcome.setCharacterSize(20);
                        leaderboardWelcome.setStyle(sf::Text::Bold | sf::Text::Underlined);
                        leaderboardWelcome.setFillColor(sf::Color::White);

                        sf::FloatRect leaderboardWelcomeRect = leaderboardWelcome.getLocalBounds();

                        leaderboardWelcome.setOrigin(leaderboardWelcomeRect.left + leaderboardWelcomeRect.width/2.0f,
                        leaderboardWelcomeRect.top + leaderboardWelcomeRect.height/2.0f);
                        leaderboardWelcome.setPosition(sf::Vector2f((colCount*16)/2.0f,(((rowCount*16)+50)/2.0f)-120));


                        while (leaderboardWindow.isOpen()) {
                            sf::Event leaderboardEvent;
                            while (leaderboardWindow.pollEvent(leaderboardEvent)) {
                                if(leaderboardEvent.type == sf::Event::Closed) {
                                    leaderboardWindow.close();
                                    paused = false;
                                }
                            }
                            leaderboardWindow.clear(sf::Color::Blue);
                            leaderboardWindow.draw(leaderboardWelcome);
                            displayLeaderboardContent(leaderboardWindow,rowCount, colCount);
                            leaderboardWindow.display();

                        }
                    }

                    if (debugBttn.getGlobalBounds().contains(mousePos.x, mousePos.y)) {
                        if (!gameEnded) { // Check if the game has ended
                            debugMode = !debugMode; // Toggle debug mode
                            if (debugMode) {
                                 // Show mines
                                // Loop through all tiles and reveal mines
                                for (int i = 0; i < rowCount - 2; ++i) {
                                    for (int j = 0; j < colCount; ++j) {
                                        if (mineLocations[i][j]) {
                                            sprites[i][j].setTexture(mineText);
                                        }
                                    }
                                }
                            }  else {
                                // Hide mines
                                // Loop through all tiles and hide mines based on their current state
                                for (int i = 0; i < rowCount - 2; ++i) {
                                    for (int j = 0; j < colCount; ++j) {
                                        if (sprites[i][j].getTexture() == &mineText) {
                                        sprites[i][j].setTexture(tileHiddenText);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    else if (row < rowCount - 2) {
                        // Change the clicked image
                        if (!mineLocations[row][col]) {
                            if (sprites[row][col].getTexture() == &tileHiddenText) {
                                revealAdjacentTiles(sprites, mineLocations, row, col, rowCount, colCount, tileHiddenText, tileRevealedText);
                            }
                           if (allNonMineTilesRevealed(sprites, mineLocations, tileHiddenText)) {
                                //YOU WIN
                                gameEnded = true;
                                happyFaceBttn.setTexture(faceWinText);
                                paused = true;
                                gameWon = true;

                            }  
                        } else {
                            // If it's a mine, reveal "mine.png"
                            sprites[row][col].setTexture(mineText);
                            for (int i = 0; i < rowCount - 2; ++i) {
                                for (int j = 0; j < colCount; ++j) {
                                    if (mineLocations[i][j]) {
                                        sprites[i][j].setTexture(mineText);
                                    }
                                }
                            }
                            if (sprites[row][col].getTexture() != &mineText) {
                                sprites[row][col].setTexture(mineText);
                            }
                            // YOU LOSE
                            happyFaceBttn.setTexture(faceLoseText);
                            paused = !paused;
                            gameActive = false;
                            gameEnded = true;
                            gameLost = true;
                        }
                        
                    } else {
                        // If clicked on the mine, reveal "mine.png"
                        sprites[row][col].setTexture(mineText);
                    }

                    if (happyFaceBounds.contains(mousePos.x, mousePos.y)) {
                        // Reset the game
                        gameEnded = false;
                        // Resets all tiles to initial state
                        for (int i = 0; i < rowCount - 2; ++i) {
                            for (int j = 0; j < colCount; ++j) {
                                sprites[i][j].setTexture(tileHiddenText);
                            }
                        }

                        for (int i = 0; i < rowCount - 2; ++i) {
                            for (int j = 0; j < colCount; ++j) {
                                mineLocations[i][j] = false;
                            }
                        }

                        // Resets mines
                        placeMines(mineLocations, numOfMines, rowCount, colCount);
                        // Resets face to "happyface" image
                        happyFaceBttn.setTexture(happyFaceText);
                    }
                
                if(pauseBttn.getGlobalBounds().contains(vec.x, vec.y)){
                    paused = !paused; //boolean changed everytime pause button is clicked

                    if(paused) {
                        cout << "Minesweeper is paused." << endl;
                        pauseTime = chrono::high_resolution_clock::now();

                    }else{
                        //unpaused
                        auto unPausedTime = chrono::high_resolution_clock::now();
                        elapsed_paused_time += (chrono::duration_cast<chrono::seconds>(unPausedTime - pauseTime)).count(); //Addition is necessary for when hitting the pause button more than once
                    }
                }

            } else if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Right) {
                    sf::Vector2i mousePos = sf::Mouse::getPosition(gameWindow);
                    int row = mousePos.y / tileSizeY;
                    int col = mousePos.x / tileSizeX;

                    if (row < rowCount - 2) {
                        if (!flaggedTiles[row][col]) {
                            // Flag the tile
                            sprites[row][col].setTexture(flagText);
                            flaggedTiles[row][col] = true;
                            countFlags--;
                        } else {
                            // Unflag the tile
                            sprites[row][col].setTexture(tileHiddenText); // Remove flag texture
                            flaggedTiles[row][col] = false;
                            countFlags++;
                        }
                    }
                }  
            }    
        }

        gameWindow.clear(sf::Color::White);

        for (const auto& row : sprites) {
            for (const auto& sprite : row) {
                gameWindow.draw(sprite);
            }
        }


        //this finds the time elapsed, so the current time - the time the window opened.
        auto game_duration = std::chrono::duration_cast<std::chrono::seconds>(chrono::high_resolution_clock::now() - start_time);
        int total_time = game_duration.count(); // necessary to subtract elapsed time later because "game_duration.count()" is const

        int minutes;
        int seconds;

        if(!paused) {
            //enters if the game is NOT paused. This is the condition that keeps the timer from incrementing when paused.
            total_time =  total_time - elapsed_paused_time; //
            minutes = total_time / 60;
            seconds = total_time % 60;
        }

        //"separating" the integers. So.... 68 -> seconds0 = 6 and seconds1 = 8
        int minutes0 = minutes / 10 % 10; //minutes index 0
        int minutes1 = minutes % 10; // minutes index 1
        int seconds0 = seconds / 10 % 10; // seconds index 0
        int seconds1 = seconds % 10; // seconds index 1


        digitsMap[minutes0].setPosition((colCount*32)- 97, 32*(rowCount + 0.5f) + 16);
        gameWindow.draw(digitsMap[minutes0]);

        digitsMap[minutes1].setPosition((colCount*32) - 76, 32*(rowCount + 0.5f) + 16);
        gameWindow.draw(digitsMap[minutes1]);

        digitsMap[seconds0].setPosition((colCount*32) - 54, 32*(rowCount + 0.5f) + 16);
        gameWindow.draw(digitsMap[seconds0]);

        digitsMap[seconds1].setPosition((colCount*32) - 33, 32*(rowCount + 0.5f) + 16);
        gameWindow.draw(digitsMap[seconds1]);

        gameWindow.draw(pauseBttn);

        if(paused){
            gameWindow.draw(playBttn);
        }

        if (gameWon == true) {
            updateLeaderboard(name, total_time);
        }


        gameWindow.draw(debugBttn);
        gameWindow.draw(leaderboardBttn);
        gameWindow.draw(happyFaceBttn);

        drawCount(gameWindow, countFlags, rowCount, digits);

        gameWindow.display();
    }
    
    
    return 0;
}