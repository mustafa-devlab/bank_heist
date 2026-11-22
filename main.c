/* Bank Heist - a console puzzle game in C */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define NUM_PUZZLES 5
#define MAX_ATTEMPTS 3
#define NUM_ABILITIES 3
#define PIN_LENGTH 4

#define EMP_JAMMER 0
#define MASTER_KEY 1
#define EXTRA_TRY 2

#define CAUGHT 0
#define ESCAPED 1
#define SCORE_FILE "high_scores.txt"
#define MAX_SCORES 100

/* ANSI colours */
#define RESET "\033[0m"
#define BOLD "\033[1m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"

typedef struct {
    char name[30];
    int difficulty;
    int alert;
    int cash;
    int reward;
    int hintsLeft;
    int abilities[NUM_ABILITIES];
    int solved;
} Player;

typedef struct {
    char name[30];
    int difficulty;
    int solved;
    int loot;
} Score;

/* used by the menus, the status panel and the score table */
const char *difficultyNames[3] = {"Easy", "Medium", "Hard"};

/* ---------- input and screen helpers ---------- */

void readLine(char *text, int size) {
    if (fgets(text, size, stdin) == NULL) {
        printf("\nInput closed. Goodbye!\n");
        exit(0);
    }
    if (strchr(text, '\n') == NULL) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF)
            ;
    }
    text[strcspn(text, "\r\n")] = '\0';
}

/* keeps asking until the number is between low and high */
int askChoice(int low, int high) {
    char text[20];
    int choice;
    while (1) {
        printf(BOLD "Choice: " RESET);
        readLine(text, sizeof(text));
        choice = atoi(text);
        if (choice >= low && choice <= high)
            return choice;
        printf(RED "Please enter a number from %d to %d.\n" RESET, low, high);
    }
}

void makeUpper(char *text) {
    int i;
    for (i = 0; text[i] != '\0'; i++)
        text[i] = toupper((unsigned char)text[i]);
}

void heading(const char *title) {
    printf("\n" CYAN BOLD "==== %s ====" RESET "\n", title);
}

/* ---------- PART 1: PUZZLES AND THE SCORE FILE ---------- */

int seq[6];
char seqHint[60];

char secretWord[20], scrambled[20];

char plainWord[20], codedWord[20];
int shiftKey;

char grid[5][5], safeRoute[9];

int lockRule, lockA, lockB, lockN, lockAnswer;

const char *bankWords[8] = {"VAULT", "MONEY", "TELLER", "LEDGER",
                            "LOCKER", "DEPOSIT", "CASHIER", "BALANCE"};
const char *codeWords[8] = {"SAFE", "GOLD", "CODE", "ALARM",
                            "GUARD", "CAMERA", "ESCAPE", "SECRET"};
const char *lockRules[3] = {"f(n) = f(n-1) + f(n-2)", "f(n) = 2 * f(n-1) + 1", "f(n) = 2 * f(n-1) - n"};
const char *puzzleNames[NUM_PUZZLES] = {"Keypad Sequence", "Scrambled Password", "Cipher Decoder",
                                        "Laser Grid", "Recursive Vault Lock"};

void makeSequence(int difficulty) {
    int kind = rand() % 2;
    int start = 2 + rand() % 8;
    int step = 2 + rand() % (6 + 4 * difficulty);
    int ratio = 2 + rand() % 2;
    int i;
    seq[0] = start;
    for (i = 1; i < 6; i++) {
        if (kind == 0)
            seq[i] = seq[i - 1] + step;
        else
            seq[i] = seq[i - 1] * ratio;
    }

    if (kind == 0)
        sprintf(seqHint, "Each number is %d more than the one before it.", step);
    else
        sprintf(seqHint, "Each number is %d times the one before it.", ratio);
}

void shuffle(char *text, int len) {
    int i, j;
    char temp;
    for (i = len - 1; i > 0; i--) {
        j = rand() % (i + 1);
        temp = text[i];
        text[i] = text[j];
        text[j] = temp;
    }
}

void makeScramble(int difficulty) {
    strcpy(secretWord, bankWords[difficulty * 2 + rand() % 4]);
    do {
        strcpy(scrambled, secretWord);
        shuffle(scrambled, strlen(secretWord));
    } while (strcmp(scrambled, secretWord) == 0);
}

void makeCipher(int difficulty) {
    int i;
    strcpy(plainWord, codeWords[difficulty * 2 + rand() % 4]);
    shiftKey = 1 + rand() % (3 + difficulty * 3);
    for (i = 0; plainWord[i] != '\0'; i++)
        codedWord[i] = (plainWord[i] - 'A' + shiftKey) % 26 + 'A';
    codedWord[i] = '\0';
}

void makeGrid(int difficulty) {
    int r, c, i;
    int laserChance = 35 + difficulty * 10;
    /* a hidden safe route: any mix of 4 right moves and 4 down moves */
    strcpy(safeRoute, "RRRRDDDD");
    shuffle(safeRoute, 8);

    for (r = 0; r < 5; r++) {
        for (c = 0; c < 5; c++)
            grid[r][c] = (rand() % 100 < laserChance) ? 'L' : '.';
    }

    /* clear every cell on the hidden route so a safe path always exists */
    r = 0;
    c = 0;
    for (i = 0; i < 8; i++) {
        if (safeRoute[i] == 'R')
            c++;
        else
            r++;
        grid[r][c] = '.';
    }
    grid[0][0] = 'S';
    grid[4][4] = 'E';
}

/* returns 1 if the moves reach the exit, 0 if not, -1 if the input is invalid */
int checkRoute(const char *moves) {
    int r = 0, c = 0, step = 0, i;
    if (strspn(moves, "UDLR ") != strlen(moves) || strspn(moves, " ") == strlen(moves))
        return -1;

    for (i = 0; moves[i] != '\0'; i++) {
        if (moves[i] == ' ')
            continue;
        step++;
        r = r + (moves[i] == 'D') - (moves[i] == 'U');
        c = c + (moves[i] == 'R') - (moves[i] == 'L');

        if (r < 0 || r > 4 || c < 0 || c > 4 || grid[r][c] == 'L') {
            printf(RED "Move %d hit a wall or a laser!\n" RESET, step);
            return 0;
        }
        if (grid[r][c] == 'E')
            return 1;
    }
    printf(RED "Your moves ended before you reached the exit.\n" RESET);
    return 0;
}

int lockValue(int n) {
    if (n == 0)
        return lockA;
    if (lockRule == 0 && n == 1)
        return lockB;
    if (lockRule == 0)
        return lockValue(n - 1) + lockValue(n - 2);
    if (lockRule == 1)
        return 2 * lockValue(n - 1) + 1;
    return 2 * lockValue(n - 1) - n;
}

void makeLock(int difficulty) {
    lockRule = rand() % 3;
    lockA = 1 + rand() % 4;
    lockB = 1 + rand() % 4;
    if (lockRule == 2)
        lockA = lockA + 2;
    lockN = 5 + difficulty + rand() % 2;
    lockAnswer = lockValue(lockN);
}

void generatePuzzle(int level, int difficulty) {
    if (level == 1)
        makeSequence(difficulty);
    else if (level == 2)
        makeScramble(difficulty);
    else if (level == 3)
        makeCipher(difficulty);
    else if (level == 4)
        makeGrid(difficulty);
    else
        makeLock(difficulty);
}

void showPuzzle(int level) {
    int i, r, c;
    printf("\n" CYAN BOLD "==== PUZZLE %d of %d: %s ====" RESET "\n", level, NUM_PUZZLES, puzzleNames[level - 1]);
    if (level == 1) {
        printf("The keypad shows a number pattern. Enter the next number.\n\n   ");
        for (i = 0; i < 5; i++)
            printf("%d   ", seq[i]);
        printf("?\n");
    } else if (level == 2) {
        printf("Unscramble these letters to get the banking-word password.\n\n   ");
        for (i = 0; scrambled[i] != '\0'; i++)
            printf("%c ", scrambled[i]);
        printf("\n");
    } else if (level == 3) {
        printf("Every letter of this word was shifted forward in the alphabet by the\n");
        printf("same secret number. Decode the original word.\n\n   %s\n", codedWord);
    } else if (level == 4) {
        printf("Cross the laser grid from S to E without touching a laser (L).\n");
        printf("Type your moves using U, D, L and R, for example RRDDRD.\n\n");
        for (r = 0; r < 5; r++) {
            printf("   ");
            for (c = 0; c < 5; c++)
                printf(grid[r][c] == 'L' ? RED "%c " RESET : "%c ", grid[r][c]);
            printf("\n");
        }
    } else {
        printf("The vault lock follows a recursive rule:\n\n");
        printf("   f(0) = %d", lockA);
        if (lockRule == 0)
            printf(", f(1) = %d", lockB);
        printf("\n   %s\n\nEnter the value of f(%d) to open the lock.\n", lockRules[lockRule], lockN);
    }
}

/* the answer is already in capital letters; returns 1, 0 or -1 (invalid format) */
int checkAnswer(int level, const char *answer) {
    if (level == 1 || level == 5) {
        if (answer[0] == '\0' || strspn(answer, "0123456789") != strlen(answer))
            return -1;
        if (level == 1)
            return atoi(answer) == seq[5];
        return atoi(answer) == lockAnswer;
    }
    if (level == 2)
        return strcmp(answer, secretWord) == 0;
    if (level == 3)
        return strcmp(answer, plainWord) == 0;
    return checkRoute(answer);
}

void getHint(int level, char *text) {
    if (level == 1)
        strcpy(text, seqHint);
    else if (level == 2)
        sprintf(text, "The word has %d letters, starts with %c and ends with %c.",
                (int)strlen(secretWord), secretWord[0], secretWord[strlen(secretWord) - 1]);
    else if (level == 3)
        sprintf(text, "The secret shift is %d. Move every letter back by %d.", shiftKey, shiftKey);
    else if (level == 4)
        sprintf(text, "A safe route starts with %c%c%c.", safeRoute[0], safeRoute[1], safeRoute[2]);
    else
        sprintf(text, "f(%d) = %d. You are only one step away.", lockN - 1, lockValue(lockN - 1));
}

/* adds one line to the score file */
void saveScore(const Player *p, int loot) {
    FILE *file = fopen(SCORE_FILE, "a");

    if (file == NULL) {
        printf(YELLOW "Could not save your score.\n" RESET);
        return;
    }
    fprintf(file, "%d %d %d %s\n", loot, p->difficulty, p->solved, p->name);
    fclose(file);
    printf(GREEN "\nScore saved to " SCORE_FILE "\n" RESET);
}

/* reads every saved score, sorts them and prints the best five */
void showHighScores(void) {
    Score list[MAX_SCORES], temp;
    char line[100];
    int count = 0, i, j;
    FILE *file = fopen(SCORE_FILE, "r");

    heading("HIGH SCORES");
    if (file != NULL) {
        while (count < MAX_SCORES && fgets(line, sizeof(line), file) != NULL) {
            if (sscanf(line, "%d %d %d %29[^\n]", &list[count].loot, &list[count].difficulty,
                       &list[count].solved, list[count].name) == 4 && list[count].difficulty >= 0 && list[count].difficulty <= 2)
                count++;
        }
        fclose(file);
    }
    if (count == 0) {
        printf("No scores saved yet. Finish a heist with some cash to get on the board!\n");
        return;
    }

    for (i = 0; i < count - 1; i++) {
        for (j = i + 1; j < count; j++) {
            if (list[j].loot > list[i].loot) {
                temp = list[i];
                list[i] = list[j];
                list[j] = temp;
            }
        }
    }
    printf(BOLD "  Rank  %-20s %-8s Puzzles  Loot" RESET "\n", "Agent", "Mode");
    for (i = 0; i < count && i < 5; i++)
        printf("  %-5d %-20s %-8s %d of %d    " MAGENTA "$%d" RESET "\n", i + 1, list[i].name,
               difficultyNames[list[i].difficulty], list[i].solved, NUM_PUZZLES, list[i].loot);
}

/* ---------- PART 2: SECURITY, REWARDS, HINTS, ABILITIES, EVENTS ---------- */

const int alertPerFail[3] = {8, 12, 16};
const int startHints[3] = {4, 3, 2};
const int startAbilities[3][NUM_ABILITIES] = {{2, 2, 2}, {1, 1, 1}, {1, 0, 1}};
const int rewardPercent[3] = {100, 150, 200};
const int eventChance[3] = {25, 35, 50};
const int baseReward[NUM_PUZZLES] = {10000, 20000, 35000, 55000, 80000};

/* the stage is the alert divided by 25: 0 to 4 */
const char *stageNames[5] = {"Quiet", "Suspicious", "Alarmed", "LOCKDOWN", "CAUGHT"};
const char *stageColors[5] = {GREEN, YELLOW, RED, RED BOLD, RED BOLD};
const char *stageWarnings[5] = {
    "",
    "Security is getting suspicious. Guards are watching the cameras.",
    "ALARMED! Guards are on the move. Hints now cost double.",
    "LOCKDOWN! The hint terminals are offline. One more slip and you are caught.",
    "ALARM! The police have been called!"
};

/* every change to the alert goes through here */
void changeAlert(Player *p, int amount) {
    int oldStage = p->alert / 25;
    int newStage;
    p->alert = p->alert + amount;
    if (p->alert < 0)
        p->alert = 0;
    if (p->alert > 100)
        p->alert = 100;

    newStage = p->alert / 25;
    if (newStage > oldStage)
        printf("%s%s" RESET "\n", stageColors[newStage], stageWarnings[newStage]);
    else if (newStage < oldStage)
        printf(GREEN "Security calms down to the %s level.\n" RESET, stageNames[newStage]);
}

/* lowers the reward at stake but never below zero */
void reduceReward(Player *p, int amount) {
    p->reward = p->reward - amount;
    if (p->reward < 0)
        p->reward = 0;
}

int fullReward(int level, int difficulty) {
    return baseReward[level - 1] * rewardPercent[difficulty] / 100;
}

void setupPlayer(Player *p, int difficulty) {
    int i;
    p->difficulty = difficulty;
    p->hintsLeft = startHints[difficulty];
    for (i = 0; i < NUM_ABILITIES; i++)
        p->abilities[i] = startAbilities[difficulty][i];
}

void failedAttempt(Player *p, int level, int attemptsLeft) {
    int cut = fullReward(level, p->difficulty) / 4;
    int increase = alertPerFail[p->difficulty] + level;
    reduceReward(p, cut);

    if (attemptsLeft == 0) {
        p->alert = 100;
        printf(RED "Wrong answer, and that was your last attempt!\n" RESET);
        return;
    }

    printf(RED "Wrong! " RESET "Security alert +%d, reward drops to " MAGENTA "$%d" RESET ".\n",
           increase, p->reward);
    changeAlert(p, increase);
}

/* returns 1 if the player may have a hint */
int buyHint(Player *p, int level) {
    int cost = fullReward(level, p->difficulty) / 10;

    if (p->alert >= 75 || p->hintsLeft == 0) {
        printf(RED "%s\n" RESET, p->alert >= 75 ? "The hint terminals are offline during lockdown." : "You have no hints left.");
        return 0;
    }
    if (p->alert >= 50)
        cost = cost * 2;

    p->hintsLeft--;
    reduceReward(p, cost);
    printf(YELLOW "The hint costs $%d." RESET " Hints left: %d. Reward is now " MAGENTA "$%d" RESET ".\n",
           cost, p->hintsLeft, p->reward);
    return 1;
}

/* shows the ability menu; returns 1 only if the puzzle was skipped */
int useAbility(Player *p, int *attemptsLeft) {
    int choice;
    printf("\n  [1] EMP Jammer - lowers the alert by 30              (left: %d)\n", p->abilities[EMP_JAMMER]);
    printf("  [2] Master Key - skips the puzzle, 40%% of the reward (left: %d)\n", p->abilities[MASTER_KEY]);
    printf("  [3] Extra Try  - gives one more attempt              (left: %d)\n", p->abilities[EXTRA_TRY]);
    printf("  [0] Cancel\n");
    choice = askChoice(0, NUM_ABILITIES);
    if (choice == 0)
        return 0;

    choice--;
    if (p->abilities[choice] == 0) {
        printf(RED "You have none of that ability left.\n" RESET);
        return 0;
    }
    p->abilities[choice]--;

    if (choice == EMP_JAMMER) {
        printf(GREEN "EMP fired! The cameras flicker and die for a moment.\n" RESET);
        changeAlert(p, -30);
    } else if (choice == EXTRA_TRY) {
        *attemptsLeft = *attemptsLeft + 1;
        printf(GREEN "Extra Try activated. Attempts left: %d.\n" RESET, *attemptsLeft);
    } else {
        p->reward = p->reward * 40 / 100;
        printf(GREEN "The Master Key opens the door, but the reward shrinks to " MAGENTA "$%d" RESET ".\n", p->reward);
        return 1;
    }
    return 0;
}

void puzzleSolved(Player *p) {
    p->cash = p->cash + p->reward;
    p->solved++;
    printf(GREEN "\n    .-------.\n    | [ O ] |   CLICK! The vault door swings open.\n    '-------'\n" BOLD
           "Puzzle solved!" RESET " You grab " MAGENTA "$%d" RESET ". Total cash: " MAGENTA "$%d" RESET "\n",
           p->reward, p->cash);
}

void randomEvent(Player *p) {
    int event, found;
    if (rand() % 100 >= eventChance[p->difficulty])
        return;

    heading("RANDOM EVENT");
    event = rand() % 3;

    if (event == 0) {
        printf("A guard walks down the corridor towards you!\n");
        if (rand() % 100 < 60) {
            printf(GREEN "You duck into a doorway and he walks past.\n" RESET);
        } else {
            printf(RED "He spots you and raises the alarm!\n" RESET);
            changeAlert(p, 20);
        }
    } else if (event == 1) {
        printf(GREEN "A power flicker blacks out half the cameras. The alert drops by 10.\n" RESET);
        changeAlert(p, -10);
    } else {
        found = 1000 + (rand() % 9) * 500;
        printf(GREEN "You find a forgotten cash drawer with " MAGENTA "$%d" GREEN " inside!\n" RESET, found);
        p->cash = p->cash + found;
    }
}

void showStatus(const Player *p) {
    int i;
    int stage = p->alert / 25;
    printf("\n" CYAN "----- STATUS: %s (%s) -----" RESET "\n", p->name, difficultyNames[p->difficulty]);
    printf("  Alert     : %s[", stageColors[stage]);
    for (i = 0; i < 10; i++)
        putchar(i < p->alert / 10 ? '#' : '-');
    printf("] %d%% %s" RESET "\n", p->alert, stageNames[stage]);
    printf("  Vaults    : ");
    for (i = 0; i < NUM_PUZZLES; i++)
        printf(i < p->solved ? GREEN "[#]" RESET : "[ ]");
    printf("\n  Cash      : " MAGENTA "$%d" RESET "   Hints left: %d\n", p->cash, p->hintsLeft);
    printf("  Abilities : EMP x%d | Master Key x%d | Extra Try x%d\n",
           p->abilities[EMP_JAMMER], p->abilities[MASTER_KEY], p->abilities[EXTRA_TRY]);
    printf(CYAN "------------------------------------" RESET "\n");
}

int pinBonus(const Player *p, int triesUsed, int triesTotal) {
    int full = 100000 * rewardPercent[p->difficulty] / 100;
    return full * (triesTotal - triesUsed + 1) / triesTotal;
}

/* runs one puzzle; returns 1 if solved, 0 if caught, 2 if the player quits */
int playPuzzle(Player *p, int level) {
    char input[100], hint[100];
    int attemptsLeft = MAX_ATTEMPTS;
    int hintShown = 0;
    int result;
    generatePuzzle(level, p->difficulty);
    p->reward = fullReward(level, p->difficulty);
    showPuzzle(level);

    while (attemptsLeft > 0) {
        printf("\nAttempts left: %d | Reward at stake: " MAGENTA "$%d" RESET "\n", attemptsLeft, p->reward);
        printf("Your answer (or HINT / ABILITY / STATUS / QUIT): ");
        readLine(input, sizeof(input));
        makeUpper(input);

        if (strcmp(input, "HINT") == 0) {
            if (hintShown) {
                printf(RED "You already used the hint for this puzzle.\n" RESET);
            } else if (buyHint(p, level)) {
                hintShown = 1;
                getHint(level, hint);
                printf(CYAN "HINT: " RESET "%s\n", hint);
            }
        } else if (strcmp(input, "ABILITY") == 0) {
            if (useAbility(p, &attemptsLeft)) {
                puzzleSolved(p);
                return 1;
            }
        } else if (strcmp(input, "STATUS") == 0) {
            showStatus(p);
        } else if (strcmp(input, "QUIT") == 0) {
            return 2;
        } else {
            result = checkAnswer(level, input);
            if (result == -1) {
                printf(RED "That is not a valid entry, so no attempt was used.\n" RESET);
            } else if (result == 1) {
                puzzleSolved(p);
                return 1;
            } else {
                attemptsLeft--;
                failedAttempt(p, level, attemptsLeft);
                if (p->alert >= 100)
                    break;
            }
        }
    }

    printf(RED BOLD "\nThe vault door slams shut and the alarms start screaming!\n" RESET);
    return 0;
}

/* ---------- PART 3: GAME FLOW AND THE FINAL PIN CHALLENGE ---------- */

int secretPin[PIN_LENGTH];
int clueSlot;
const int pinTries[3] = {8, 6, 5};
const int rankLimits[4] = {80, 60, 30, 1};
const char *rankNames[5] = {"Legendary Mastermind", "Master Thief", "Skilled Burglar", "Petty Thief", "Rookie"};

void makePin(void) {
    char digits[11] = "0123456789";
    int i;
    shuffle(digits, 10);
    for (i = 0; i < PIN_LENGTH; i++)
        secretPin[i] = digits[i] - '0';
    clueSlot = rand() % PIN_LENGTH;
}

void showClue(int number) {
    int i, sum = 0, big = 0, small = 9;
    for (i = 0; i < PIN_LENGTH; i++) {
        sum = sum + secretPin[i];
        if (secretPin[i] > big)
            big = secretPin[i];
        if (secretPin[i] < small)
            small = secretPin[i];
    }

    printf("  " CYAN "Clue %d:" RESET " ", number);
    if (number == 1)
        printf("The digits are all different and add up to %d.\n", sum);
    else if (number == 2)
        printf("The first digit is %s.\n", secretPin[0] % 2 == 0 ? "even" : "odd");
    else if (number == 3)
        printf("The biggest digit is %d.\n", big);
    else if (number == 4)
        printf("The smallest digit is %d.\n", small);
    else
        printf("Digit number %d is %d.\n", clueSlot + 1, secretPin[clueSlot]);
}

int readGuess(int guess[]) {
    char input[50];
    int i, j;
    readLine(input, sizeof(input));
    if (strlen(input) != PIN_LENGTH || strspn(input, "0123456789") != PIN_LENGTH)
        return 0;
    for (i = 0; i < PIN_LENGTH; i++)
        guess[i] = input[i] - '0';
    for (i = 0; i < PIN_LENGTH; i++) {
        for (j = i + 1; j < PIN_LENGTH; j++) {
            if (guess[i] == guess[j])
                return 0;
        }
    }
    return 1;
}

void scoreGuess(const int guess[], int *exact, int *close) {
    int i, j;
    *exact = 0;
    *close = 0;
    for (i = 0; i < PIN_LENGTH; i++) {
        for (j = 0; j < PIN_LENGTH; j++) {
            if (guess[i] == secretPin[j]) {
                if (i == j)
                    (*exact)++;
                else
                    (*close)++;
            }
        }
    }
}

void finalPinChallenge(Player *p) {
    int guess[PIN_LENGTH];
    int tries = pinTries[p->difficulty];
    int used = 0, exact, close, bonus, i;
    heading("THE MANAGER'S SAFE");
    printf("One safe is left, locked with a %d-digit PIN. Your notes so far:\n", PIN_LENGTH);
    for (i = 1; i <= NUM_PUZZLES; i++)
        showClue(i);
    printf("\nYou have %d tries. The sooner you open it, the bigger the bonus.\n", tries);

    while (used < tries) {
        printf("\nGuess %d of %d (4 different digits): ", used + 1, tries);
        if (!readGuess(guess)) {
            printf(RED "Enter exactly 4 different digits. That did not use a try.\n" RESET);
            continue;
        }
        used++;
        scoreGuess(guess, &exact, &close);

        if (exact == PIN_LENGTH) {
            bonus = pinBonus(p, used, tries);
            p->cash = p->cash + bonus;
            printf(GREEN "\n    .---------.\n    | $ $ $ $ |   The safe clicks open!\n    '---------'\n" RESET
                   "PIN bonus: " MAGENTA "$%d" RESET "\n", bonus);
            return;
        }
        printf(YELLOW "%d in the right place, %d correct but in the wrong place.\n" RESET, exact, close);
    }

    printf(RED "\nThe safe stays shut. The PIN was ");
    for (i = 0; i < PIN_LENGTH; i++)
        printf("%d", secretPin[i]);
    printf("." RESET "\n");
}

void showHowToPlay(void) {
    heading("HOW TO PLAY");
    printf("  1. Each puzzle gives you %d attempts.\n", MAX_ATTEMPTS);
    printf("  2. A wrong answer raises the alert and cuts the reward by 25%%.\n");
    printf("  3. At 25%% alert guards get suspicious, at 50%% hints cost double,\n");
    printf("     at 75%% hints stop working, and at 100%% you are caught.\n");
    printf("  4. Type HINT, ABILITY, STATUS or QUIT instead of an answer.\n");
    printf("  5. After each puzzle, go deeper or escape. If caught, you lose it all.\n");
    printf("  6. Each solved puzzle reveals a PIN clue. Solve all five to guess the PIN.\n");
}

void showSummary(const Player *p, int outcome) {
    int loot = p->cash;
    int best = 300000 * rewardPercent[p->difficulty] / 100; /* all puzzles plus the full PIN bonus */
    int percent, rank = 0;
    if (outcome == CAUGHT) {
        printf(RED "\n    |||  |||  |||  |||\n    |||   " BOLD "BUSTED!" RESET RED "  |||\n    |||  |||  |||  |||\n"
               BOLD "  The police arrest you and take the loot.\n" RESET);
        loot = 0;
    } else {
        printf(GREEN BOLD "\n  You slip out of the bank with your cash.\n" RESET);
    }

    percent = loot * 100 / best;
    while (rank < 4 && percent < rankLimits[rank])
        rank++;

    if (loot > 0)
        saveScore(p, loot);

    heading("HEIST REPORT");
    printf("  Agent           : %s (%s mode)\n", p->name, difficultyNames[p->difficulty]);
    printf("  Puzzles solved  : %d of %d\n", p->solved, NUM_PUZZLES);
    printf("  Final alert     : %d%%\n", p->alert);
    printf("  Final loot      : " MAGENTA "$%d" RESET "\n", loot);
    printf("  Rank            : " BOLD "%s" RESET "\n", rankNames[rank]);
}

void playGame(void) {
    Player p = {0};
    int level, result;
    int outcome = ESCAPED;
    printf("\n" BOLD "Enter your codename: " RESET);
    readLine(p.name, sizeof(p.name));
    if (p.name[0] == '\0')
        strcpy(p.name, "Agent X");

    heading("CHOOSE A DIFFICULTY");
    printf("  [1] Easy   - mild alerts, 4 hints, 2 of each ability, normal rewards\n");
    printf("  [2] Medium - stronger alerts, 3 hints, 1 of each ability, rewards x1.5\n");
    printf("  [3] Hard   - harsh alerts, 2 hints, no Master Key, rewards x2\n");
    setupPlayer(&p, askChoice(1, 3) - 1);
    makePin();

    printf("\nIt is midnight, %s. The City Bank is silent.\n", p.name);
    printf("Five vault doors stand between you and the cash. Do not trip the alarm.\n");

    for (level = 1; level <= NUM_PUZZLES; level++) {
        if (level > 1) {
            randomEvent(&p);
            if (p.alert >= 100) {
                printf(RED "The guards have found you!\n" RESET);
                outcome = CAUGHT;
                break;
            }
        }

        result = playPuzzle(&p, level);
        if (result == 0) {
            outcome = CAUGHT;
            break;
        }
        if (result == 2)
            break;

        printf("\nA note is taped inside the vault door:\n");
        showClue(level);

        if (level < NUM_PUZZLES) {
            showStatus(&p);
            printf("\nWhat now?  [1] Go deeper   [2] Escape with " MAGENTA "$%d" RESET "\n", p.cash);
            if (askChoice(1, 2) == 2) {
                outcome = ESCAPED;
                break;
            }
        }
    }

    if (level > NUM_PUZZLES)
        finalPinChallenge(&p);
    showSummary(&p, outcome);
}

int main(void) {
    int choice;

#ifdef _WIN32
    system(""); /* switches on colour support in the Windows console */
#endif
    srand((unsigned)time(NULL));

    printf(YELLOW "\n        ______________\n"
           "       /  $  BANK  $  \\\n"
           "      /________________\\\n"
           "       |  []  []  []  |\n"
           "     __|______________|__" RESET "\n");
    heading("B A N K   H E I S T");

    do {
        printf("\n  [1] Start the heist\n  [2] How to play\n  [3] High scores\n  [4] Exit\n");
        choice = askChoice(1, 4);
        if (choice == 1)
            playGame();
        else if (choice == 2)
            showHowToPlay();
        else if (choice == 3)
            showHighScores();
    } while (choice != 4);

    printf("\nThanks for playing. Goodbye!\n");
    return 0;
}