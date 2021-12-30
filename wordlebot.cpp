#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <array>

enum GuessResult {
    DOES_NOT_EXIST = 0,
    EXISTS_IN_DIFFERENT_SPOT = 1,
    CORRECT = 2,
};



struct FiveLetterWord {
    char word[5];
    FiveLetterWord(std::string in = "     ") {
        if(in.size() != 5) throw ("word is not 5 letters: "+in);
        for(int i = 0; i<5; i++) word[i] = in[i];
    }

    char& operator[](int i) { return word[i]; }
    const char& operator[](int i) const { return word[i]; }
    bool operator==(const FiveLetterWord& rhs) const { return word[0]==rhs.word[0] && word[1]==rhs.word[1] && word[2]==rhs.word[2] && word[3]==rhs.word[3] && word[4]==rhs.word[4]; }
    bool operator!=(const FiveLetterWord& rhs) const { return word[0]!=rhs.word[0] || word[1]!=rhs.word[1] || word[2]!=rhs.word[2] || word[3]!=rhs.word[3] || word[4]!=rhs.word[4]; }

    std::string to_s() const {
        return std::string(word, word+5);
    }

    std::string to_squares() const {
        std::string res;

        for(int i = 0; i<5; i++) {
            if(word[i]==DOES_NOT_EXIST) res += "x";
            if(word[i]==EXISTS_IN_DIFFERENT_SPOT) res += "Y";
            if(word[i]==CORRECT) res += "G";
        }

        return res;
    }

    bool is_correct() const {
        for(int i = 0; i<5; i++) {
            if(word[i]!=CORRECT) return false;
        }

        return true;
    }

    int to_score() const {
        int res = 0;
        for(int i = 0; i<5; i++) res += word[i];
        return res;
    }
};

typedef FiveLetterWord WordHint;

WordHint from_hint(std::string hint) {
    WordHint res;
    for(int i = 0; i<5; i++) {
        if(hint[i] == 'x' || hint[i] == 'X') res[i] = DOES_NOT_EXIST;
        if(hint[i] == 'y' || hint[i] == 'Y') res[i] = EXISTS_IN_DIFFERENT_SPOT;
        if(hint[i] == 'g' || hint[i] == 'G') res[i] = CORRECT;
    }
    return res;
}

WordHint evaluate_guess(const FiveLetterWord& guess, FiveLetterWord actual) {
    WordHint result = {{DOES_NOT_EXIST, DOES_NOT_EXIST, DOES_NOT_EXIST, DOES_NOT_EXIST, DOES_NOT_EXIST}};

    //do green squares
    for(int i = 0; i<5; i++) {
        if(guess[i] == actual[i]) {
            result[i] = CORRECT;
            actual[i] = 0;
        }
    }

    //do yellow squares
    for(int i = 0; i<5; i++) {
        if(result[i] != CORRECT) { //check the ones that were not marked as correct
            for(int j = 0; j<5; j++) {
                if(guess[i] == actual[j]) {
                    result[i] = EXISTS_IN_DIFFERENT_SPOT;
                    actual[j] = 0;
                    break;
                }
            }
        }
    }

    return result;
}


bool IsWordPossible(const WordHint& hint, const FiveLetterWord& guess, FiveLetterWord word) {
    //check greens
    for(int i = 0; i<5; i++) {
        if(hint[i] == CORRECT) {
            if(guess[i] != word[i]) return false;
            word[i] = 0; //for yellows
        }
    }
    //check yellows
    for(int i = 0; i<5; i++) {
        if(hint[i] == EXISTS_IN_DIFFERENT_SPOT) {
            if(guess[i] == word[i]) return false; //this would have been green, not yellow, so it fails

            bool found = false;
            for(int j = 0; j<5; j++) {
                if(guess[i] == word[j]) {
                    found = true;
                    word[j] = 0; //for yellows
                    break;
                }
            }
            if(!found) return false;
        }
    }
    //check greys
    for(int i = 0; i<5; i++) {
        if(hint[i] == DOES_NOT_EXIST) {
            for(int j = 0; j<5; j++) {
                if(guess[i] == word[j]) {
                    return false;
                }
            }
        }
    }

    return true;
}

std::vector<FiveLetterWord> FilterWordList(const WordHint& hint, const FiveLetterWord& guess, const std::vector<FiveLetterWord>& wordlist) {
    std::vector<FiveLetterWord> res;

    for(auto& word : wordlist) {
        if(IsWordPossible(hint, guess, word)) res.push_back(word);
    }

    return res;
}

int FilteredWordListSize(const WordHint& hint, const FiveLetterWord& guess, const std::vector<FiveLetterWord>& wordlist) {
    int res = 0;

    for(auto& word : wordlist) {
        if(IsWordPossible(hint, guess, word)) res += 1;
    }

    return res;
}

FiveLetterWord BestGuess_Simple(const std::vector<FiveLetterWord>& possible_guesses, const std::vector<FiveLetterWord>& possible_solutions, bool show_progress = true) {
    int best_average_score = 0;
    FiveLetterWord best_average_word;

    for(int i = 0; i<possible_guesses.size(); i++) {
        auto& guess = possible_guesses[i];
        int avg_score = 0;
        for(int j = 0; j<possible_solutions.size(); j++) {
            auto& actual = possible_solutions[j];
            avg_score += evaluate_guess(guess, actual).to_score();
        }

        if(avg_score > best_average_score) {
            best_average_score = avg_score;
            best_average_word = guess;
        }
    }

    return best_average_word;
}

FiveLetterWord BestGuess_Complex(const std::vector<FiveLetterWord>& possible_guesses, const std::vector<FiveLetterWord>& possible_solutions, bool show_progress = true) {
    constexpr int nthreads = 16;
    std::array<int, nthreads> best_average_score;
    std::array<int, nthreads> best_average_index;
    for(int i = 0; i<nthreads; i++) {
        best_average_score[i] = INT_MAX;
    }

    std::atomic<int> words_processed(0);
    std::atomic<int> threads_done(0);
    std::vector<std::thread> threads;

    for(int threadindex = 0; threadindex<nthreads; threadindex++) {
        threads.push_back(std::thread([&, threadindex]() {
            for(int i = threadindex; i<possible_guesses.size(); i += nthreads) {
                auto& guess = possible_guesses[i];
                int avg_score = 0;

                for(int j = 0; j<possible_solutions.size(); j++) {
                    auto& actual = possible_solutions[j];
                    if(guess != actual) {
                        auto hint = evaluate_guess(guess, actual);
                        int score = FilteredWordListSize(hint, guess, possible_solutions);
                        avg_score += score;//std::max(score, avg_score); 
                    }
                }

                if(avg_score < best_average_score[threadindex]) {
                    best_average_score[threadindex] = avg_score;
                    best_average_index[threadindex] = i;
                }

                ++words_processed;
            }

            ++threads_done;
        }));
    }

    //wait for threads to finish
    if(show_progress) {
        while(threads_done != nthreads) {
            std::cout << "guesses processed: " << words_processed << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }

    //join threads
    for(auto& thread : threads) thread.join();

    //select best word
    int best_index = best_average_index[0];
    int best_score = best_average_score[0];
    for(int i = 1; i<nthreads; i++) {
        if(best_average_score[i]<=best_score || (best_average_score[i]==best_score && best_average_index[i] < best_index)) {
            best_index = best_average_index[i];
            best_score = best_average_score[i];
        }
    }
    

    return possible_guesses[best_index];
}

std::vector<FiveLetterWord> LoadWordList(std::string filename) {
    std::vector<FiveLetterWord> res;
    std::ifstream file(filename);
    std::string str;
    while(std::getline(file, str)) {
        for(auto& c: str) c = toupper(c);
        res.push_back(str);
    }
    return res;
}

FiveLetterWord BestGuess(const std::vector<FiveLetterWord>& possible_guesses, const std::vector<FiveLetterWord>& possible_solutions, bool show_progress = true) {
    if(possible_solutions.size() == 1) return possible_solutions[0];
    return BestGuess_Complex(possible_guesses, possible_solutions, show_progress);
    //return BestGuess_Simple(possible_guesses, possible_solutions, show_progress);
}

void WordleBot(bool recompute_initial_guess = false) {
    std::vector<FiveLetterWord> all_words = LoadWordList("wordlist_guesses.txt");
    std::vector<FiveLetterWord> possible_words = LoadWordList("wordlist_solutions.txt");

    FiveLetterWord guess("ROATE");
    if(recompute_initial_guess) guess = BestGuess(all_words, possible_words);


    bool solved = false;

    while(!solved) {
        std::cout << "Guess: "+guess.to_s() << std::endl;
        std::cout << "Enter Result: ";
        std::string hintstr;
        std::cin >> hintstr;

        if(hintstr.size() != 5) {
            std::cout << "error in hint, aborting" << std::endl;
            break;
        }

        WordHint hint = from_hint(hintstr);
        possible_words = FilterWordList(hint, guess, possible_words);

        if(possible_words.size() == 0) {
            std::cout << "impossible, no words left to guess" << std::endl;
            break;
        }


        std::cout << "Possible Words: ";
        for(auto word : possible_words) {
            std::cout << word.to_s() << " ";
        }
        std::cout << std::endl;

        if(hint.is_correct()) {
            std::cout << "You did it!" << std::endl;
            break;
        }

        guess = BestGuess(all_words, possible_words, false);
    }
}

int WordleGame(FiveLetterWord solution, const std::vector<FiveLetterWord>& all_words, const std::vector<FiveLetterWord>& possible_words, bool output = false) { //returns number of guesses needed to get the word
    FiveLetterWord guess("ROATE");
    std::vector<FiveLetterWord> solutions;

    int guesses = 0;
    while(true) {
        ++guesses;
        if(output) std::cout << "Guess: " << guess.to_s() << std::endl;
        auto hint = evaluate_guess(guess, solution);


        if(output) std::cout << "Result: " << hint.to_squares() << std::endl;
        if(hint.is_correct()) break;

        if(guesses == 1) {
            solutions = FilterWordList(hint, guess, possible_words);
        } else {
            solutions = FilterWordList(hint, guess, solutions);
        }

        if(output) {
            for(auto word : solutions) {
                std::cout << word.to_s() << " ";
            }
            std::cout << std::endl;
        }

        FiveLetterWord new_guess = BestGuess(all_words, solutions, false);

        if(new_guess == guess) {
            guesses = 9999;
            break;
        }
        guess = new_guess;
    }

    return guesses;
}

void WordleBenchmark() {
    std::vector<FiveLetterWord> all_words = LoadWordList("wordlist_guesses.txt");
    std::vector<FiveLetterWord> possible_words = LoadWordList("wordlist_solutions.txt");

    int total_guesses = 0;
    int worst_case = 0;
    FiveLetterWord worst_word;

    for(auto& word : possible_words) { //play every possible game
        int score = WordleGame(word, all_words, possible_words);
        std::cout << word.to_s() << " took "<<score<<" guesses" << std::endl;

        if(score > worst_case) {
            worst_case = score;
            worst_word = word;
        }
        total_guesses += score;
    }

    std::cout << "Average Guesses: " <<(total_guesses/(double)possible_words.size()) << std::endl;
    std::cout << "Worst Word: " << worst_word.to_s() << " ("<<worst_case<<" guesses)"<<std::endl;
}

int main() {
    WordleBot();
    //WordleBenchmark();

    //std::vector<FiveLetterWord> all_words = LoadWordList("wordlist_guesses.txt");
    //std::vector<FiveLetterWord> possible_words = LoadWordList("wordlist_solutions.txt");
    //WordleGame(FiveLetterWord("TAPIR"), all_words, possible_words, true);

    return 0;
}