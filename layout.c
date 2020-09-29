/* Copyright (c) 2012-2015, Paul Meredith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above 
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH 
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT, 
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING 
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, 
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "graphs.h"

#define N_KEYS 47
#define N_LETTERS 26

/* What percentage of work each finger should do (pinky to index) */
const double FINGERS[8] = { 0.09, 0.13, 0.14, 0.14, 0.14, 0.14, 0.13, 0.09 };

/*  Qwerty layout
 *  esc  1   2   3   4   5   6   7   8   9   0   -   =   ~   | 
 *  -tab   q   w   e   r   t   y   u   i   o   p   [   ]   del
 *  -bksp   a   s   d   f   g   h   j   k   l   ;   '   enter-
 *  ---ctrl   z   x   c   v   b   n   m   ,   .   /   shift---
 *       meta  alt-  --------space-------- altgr  meta
 */
int qwerty[N_LETTERS] = { 26, 41, 39, 28, 16, 29, 30, 31, 21, 32, 33, 34, 43, 42, 22, 23, 14, 17, 27, 18, 20, 40, 15, 38, 19, 37 };

/*  Current layout
 *  esc  .   %   /   .   .   .   .   .   v   z   .   .   ~   . 
 *  -tab   .   y   a   &   .   q   f   l   g   k   j   .   del
 *  -bksp   i   e   o   n   ,   b   h   t   s   c   d   enter-
 *  ---ctrl   _   -   '   u   ;   r   x   w   m   p   shift---
 *       meta  alt-  --------space-------- altgr  meta
 */
int ieon[N_LETTERS] = { 16, 31, 35, 36, 27, 20, 22, 32, 26, 24, 23, 21, 45, 29, 28, 46, 19, 42, 34, 33, 40, 8, 43, 44, 15, 9 };

/*  Key number reference
 *  esc  0   1   2   3   4   5   6   7   8   9   10  11  12  13 
 *  -tab   14  15  16  17  18  19  20  21  22  23  24  25   del
 *  -bksp   26  27  28  29  30  31  32  33  34  35  36   enter-
 *  ---ctrl   37  38  39  40  41  42  43  44  45  46   shift---
 *       meta  alt-  --------space-------- altgr  meta
 */

double x[N_KEYS] = {
     0,  19,  38,  57,  76,  95, 114, 133, 152, 171, 190, 209, 228, 247,
       9,  28,  47,  66,  85, 104, 123, 142, 161, 180, 199, 217,
        14,  33,  52,  71,  90, 109, 128, 147, 166, 185, 204,
          23,  42,  61,  80,  99, 118, 137, 156, 175, 194,
};

/* The keys the fingers rest on while not typing. */
const int HOME[N_KEYS] = { [26] = 1, 1, 1, 1, 0, 0, 1, 1, 1, 1 };

/* The keys your fingers can rest and reach without interfering with other fingers. */
const int NATURAL[N_KEYS] = { [15] = 1, 1, [21] = 1, 1, [26] = 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, [40] = 1, 0, 1, [46] = 1 };

/* The hand used to press a key. */
const int HAND[N_KEYS] = {
     0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,
       0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,
        0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,
          0,  0,  0,  0,  0,  1,  1,  1,  1,  1,
};

/* The key index used for each finger on the home row. */
const int FKEY[8] = { 26, 27, 28, 29, 32, 33, 34, 35 };

/* pinky 0, ring 1, middle 2, index 3 */
const int FINGER[N_KEYS] = {
     1,  1,  2,  2,  3,  3,  3,  2,  2,  2,  1,  1,  1,  1,
       0,  1,  2,  3,  3,  3,  3,  2,  1,  0,  0,  0,
        0,  1,  2,  3,  3,  3,  3,  2,  1,  0,  0,
          1,  2,  3,  3,  3,  3,  3,  2,  1,  0,
};

/* left pinky 0 to right pinky 7 */
const int ABS_FINGER[N_KEYS] = {
     1,  1,  2,  2,  3,  3,  4,  5,  5,  5,  6,  6,  6,  6,
       0,  1,  2,  3,  3,  4,  4,  5,  6,  7,  7,  7,
        0,  1,  2,  3,  3,  4,  4,  5,  6,  7,  7,
          1,  2,  3,  3,  3,  4,  4,  5,  6,  7,
};

/* An optimization to avoid division. */
const int ROW[N_KEYS] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
       1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
        2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
          3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
};

int **five_strings;
int row[N_KEYS][N_KEYS];
int hand[N_KEYS][N_KEYS];
int finger[N_KEYS][N_KEYS];
double distance_abs[N_KEYS];

/* The number of elements in each graph array. */
const int count1 = sizeof(one_counts)/sizeof(one_counts[0]);
const int count5 = sizeof(five_counts)/sizeof(five_counts[0]);

/* The total number of occurrences of ngraphs in the English Corpus. */
double total_1graphs, total_5graphs;

/* These are offsets for printing the layout rows. */
int gaps[4] = {4, 7, 8, 6};

pthread_mutex_t p_lock;
int gen = 0;
long long layouts = 0;
int best_keymap[N_LETTERS];
double best_fitness = -10000000.0;

time_t start;

void get_fitness(int *keymap, double *fitness, int print)
{
    int i, a, b, c, d, e;
    double inrolls = 0.0, row_jumps = 0.0, same_finger = 0.0, four_same_hand = 0.0, five_same_hand = 0.0, distance = 0.0;

    /* Calculate the distance. */
    for(i = 0; i < N_LETTERS; i++) if(HOME[keymap[i]] != 1) distance += one_counts[i]*distance_abs[keymap[i]];
    
    /* Calculate finger inequality. */
    double fingers[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for(i = 0; i < N_LETTERS; i++) fingers[ABS_FINGER[keymap[i]]] += one_counts[i];
    double finger_inequality = 0.0; 
    for(i = 0; i < 8; i++) finger_inequality += fabs(FINGERS[i] - fingers[i]);
    
    for(i = 0; i < count5; i++)
    {
        a = keymap[five_strings[i][0]];
        b = keymap[five_strings[i][1]];
        c = keymap[five_strings[i][2]];
        d = keymap[five_strings[i][3]];
        e = keymap[five_strings[i][4]];
        
        if(hand[b][c] && hand[c][d])
        {
            if(hand[a][b])
            {
                if(hand[d][e]) five_same_hand += five_counts[i];
                else four_same_hand += five_counts[i];
            }
            else if(hand[d][e]) four_same_hand += five_counts[i];
        }

        if     (finger[a][b]) same_finger += five_counts[i];
        else if(finger[a][c]) same_finger += five_counts[i] / 2;
        else if(finger[a][d]) same_finger += five_counts[i] / 4;
        else if(finger[a][e]) same_finger += five_counts[i] / 8;
        if     (finger[b][c]) same_finger += five_counts[i];
        else if(finger[b][d]) same_finger += five_counts[i] / 2;
        else if(finger[b][e]) same_finger += five_counts[i] / 4;
        if     (finger[c][d]) same_finger += five_counts[i];
        else if(finger[c][e]) same_finger += five_counts[i] / 2;
        if     (finger[d][e]) same_finger += five_counts[i];
        
        if     (hand[a][b] && row[a][b] != 0) row_jumps += five_counts[i] / (1 << (3 - row[a][b]));
        else if(hand[a][c] && row[a][c] != 0) row_jumps += five_counts[i] / (1 << (4 - row[a][c]));
        else if(hand[a][d] && row[a][d] != 0) row_jumps += five_counts[i] / (1 << (5 - row[a][d]));
        else if(hand[a][e] && row[a][e] != 0) row_jumps += five_counts[i] / (1 << (6 - row[a][e]));
        if     (hand[b][c] && row[b][c] != 0) row_jumps += five_counts[i] / (1 << (3 - row[b][c]));
        else if(hand[b][d] && row[b][d] != 0) row_jumps += five_counts[i] / (1 << (4 - row[b][d]));
        else if(hand[b][e] && row[b][e] != 0) row_jumps += five_counts[i] / (1 << (5 - row[b][e]));
        if     (hand[c][d] && row[c][d] != 0) row_jumps += five_counts[i] / (1 << (3 - row[c][d]));
        else if(hand[c][e] && row[c][e] != 0) row_jumps += five_counts[i] / (1 << (4 - row[c][e]));
        if     (hand[d][e] && row[d][e] != 0) row_jumps += five_counts[i] / (1 << (3 - row[d][e]));
    
        int p[5] = {a, b, c, d, e}; 
        for(int m = 0; m < 4; m++)
        {
            for(int n = m + 1; n < 5; n++)
            {
                if(HAND[p[m]] == HAND[p[n]] && FINGER[p[n]] > FINGER[p[m]] && NATURAL[p[m]] == 1 && NATURAL[p[n]] == 1 && row[p[m]][p[n]] <= 1)
                {
                    inrolls += five_counts[i] / (1 << (n - m - 1));
                    break;
                }
            }
        }
    }

    if(print)
    {
        fprintf(stderr, "\033[5;0H d:%7.4f fi:%7.4f sf:%7.4f 4r:%7.4f 5r:%7.4f rj:%7.4f ir:%7.4f", -distance/26, -finger_inequality, -same_finger, -four_same_hand, -five_same_hand, -row_jumps, inrolls*0.5);
    }
    else
    {
        *fitness = inrolls*0.5 - (distance/26) - finger_inequality - same_finger - four_same_hand - five_same_hand - row_jumps;
    }
}

void lock()  { pthread_mutex_lock(&p_lock);   }
void unlock(){ pthread_mutex_unlock(&p_lock); }

void *loop(void *argv)
{
    int i, j, k, l, a, b;
    double fitness = -10000000.0;
    double gen_best_fitness = -10000000.0;
    int keymap[N_LETTERS];
    int gen_best_keymap[N_LETTERS];
    int local_gen;
    
    while(1)
    {
        lock();
        local_gen = gen++;
        printf("\033[7;0Hgen: %6d", gen);
        fflush(stdout);
        unlock();
        
        /* This creates a new keymap. */
        int map[N_KEYS];
        /* Fill with 0, 1, 2, ..., n_keys. */
        for(i = 0; i < N_KEYS; i++) map[i] = i;
        
        /* Fisher-Yates shuffle. */
        for(i = 0; i < N_KEYS - 2; i++)
        {
            int buf = map[j = arc4random_uniform(N_KEYS - i) + 1];
            map[j] = map[i];
            map[i] = buf;
        }
        /* Copy only the first N_LETTERS ints to the keymap. */
        memcpy(keymap, map, sizeof(int)*N_LETTERS);
        memcpy(gen_best_keymap, keymap, sizeof(int)*N_LETTERS);
        get_fitness(keymap, &gen_best_fitness, 0);
        
        for(j = 0; j < 4; j++)
        {
            for(i = 0; i < 2048; i++)
            {
                /* Shuffle the gen_best and create new locals. */
                memcpy(keymap, gen_best_keymap, sizeof(int)*N_LETTERS);
                for(k = arc4random_uniform(j); k >= 0; k--)
                {
                    // I want to take any key value from the first half of the keymap, and change it to any other key.
                    a = arc4random_uniform(N_LETTERS);
                    do b = arc4random_uniform(N_KEYS); while(b == a);

                    // Is there something on b already that we need to switch to p?
                    for(l = 0; l < N_LETTERS; l++)
                    {
                        if(keymap[l] == b)
                        {
                            keymap[l] = keymap[a];
                            break;
                        }
                    }
                    keymap[a] = b;
                }
                
                /* Calculate the fitness of the new layout. */
                get_fitness(keymap, &fitness, 0);
                
                /* If the new layout is better than the best in the generation. */
                if(fitness > gen_best_fitness)
                {
                    /* Save the layout. */
                    memcpy(gen_best_keymap, keymap, sizeof(int)*N_LETTERS);
                    gen_best_fitness = fitness;
                    
                    /* If the new layout is the best found. */
                    lock();
                    if(fitness > best_fitness)
                    {
                        best_fitness = fitness;
                        memcpy(best_keymap, keymap, sizeof(int)*N_LETTERS);
    
                        printf("\033[5;0H\033[1J");
                        for(k = 0; k < N_LETTERS; k++) 
                        {
                            a = keymap[k];
                            printf("\033[%d;%dH%c", ROW[a] + 1, gaps[ROW[a]] + (int)floor(x[a]/19.0)*5, k + 97);
                        }
                        get_fitness(keymap, &fitness, 1);
                        printf("\033[6;0Hscore: %+9.6f, gen: %4d",best_fitness, local_gen);
                        fflush(stdout);
                    }
                    unlock();
                    j = 0;
                }
            }
            lock();
            layouts += 2048;
            fprintf(stderr, "\033[7;14Hlayouts: %10llu (%4.0f l/s)",layouts, layouts/difftime(time(NULL), start));
            unlock();
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int i;
    for(i = 0; i < count1; i++) total_1graphs += one_counts[i];
    for(i = 0; i < count5; i++) total_5graphs += five_counts[i];

    for(i = 0; i < count1; i++) one_counts[i]  /= total_1graphs;
    for(i = 0; i < count5; i++) five_counts[i] /= total_5graphs;

    /* This is because the abs of these values is calculated billions of times so we use this array. */
    for(i = 0; i < N_KEYS; i++)
    {
        for(int j = 0; j < N_KEYS; j++)
        {
            row[i][j] = abs(ROW[j] - ROW[i]);
            finger[i][j] = ABS_FINGER[i] == ABS_FINGER[j];
            hand[i][j] = HAND[i] == HAND[j];
        }
    }

    /* Calculate the distance between all keys to save sqrts and pows. */
    for(i = 0; i < N_KEYS; i++)
    {
        int home_key = FKEY[ABS_FINGER[i]];
        distance_abs[i] = sqrt(pow((x[home_key] - x[i])*1.5, 2.0) + pow((ROW[home_key] - ROW[i])*19.0, 2.0));
    }

    five_strings =  malloc(sizeof(int *) * count5);
    for(i = 0; i < count5; i++)
    {
        five_strings[i] = malloc(sizeof(int)*5);
        for(int j = 0; j < 5; j++)
        {
            five_strings[i][j] = five_strings_char[i][j] - 97;
        }
    }

    printf("\033[2J");
    time(&start);
    
    pthread_mutex_init(&p_lock, NULL);
    pthread_t pid[8];
    for(i = 0; i < 8; i++) pthread_create(&(pid[i]), NULL, &loop, NULL);
    for(i = 0; i < 8; i++) pthread_join(pid[i], NULL);
    pthread_mutex_destroy(&p_lock);
    free(five_strings);
    return 0;
}
