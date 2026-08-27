#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#define USD_TO_EUR 0.85
#define EUR_TO_USD 1.18
#define USD_TO_GBP 0.75
#define GBP_TO_USD 1.33
#define EUR_TO_GBP 0.88
#define GBP_TO_EUR 1.14
#define PORT 8080
#define BUFFER_SIZE 1024

pthread_mutex_t account_mutex = PTHREAD_MUTEX_INITIALIZER;

void save_accounts_to_file();

typedef struct {
    char username[50];
    double usd_balance;
    double eur_balance;
    double gbp_balance;
} Account;

Account accounts[10]; 
int account_count = 0; 

Account* find_account(const char* username) {
    for (int i = 0; i < account_count; ++i) {
        if (strcmp(accounts[i].username, username) == 0) {
            return &accounts[i];
        }
    }
    return NULL;
}

void create_account(const char* username) {
    if (account_count < 10) {
        strcpy(accounts[account_count].username, username);
        accounts[account_count].usd_balance = 0;
        accounts[account_count].eur_balance = 0;
        accounts[account_count].gbp_balance = 0;
        account_count++;
        save_accounts_to_file();
    }
}

void save_accounts_to_file() {
    FILE *file = fopen("accounts.dat", "wb");
    if (file) {
        fwrite(&accounts, sizeof(Account), account_count, file);
        fclose(file);
    }
}

void load_accounts_from_file() {
    FILE *file = fopen("accounts.dat", "rb");
    if (file) {
        account_count = fread(&accounts, sizeof(Account), 10, file);
        fclose(file);
    }
}

void exchange_currency(Account* acc, const char* from_currency, const char* to_currency, double amount) {
    double converted_amount = 0;
    if (strcmp(from_currency, "USD") == 0 && strcmp(to_currency, "EUR") == 0) {
        converted_amount = amount * USD_TO_EUR;
        acc->usd_balance -= amount;
        acc->eur_balance += converted_amount;
    } else if (strcmp(from_currency, "EUR") == 0 && strcmp(to_currency, "USD") == 0) {
        converted_amount = amount * EUR_TO_USD;
        acc->eur_balance -= amount;
        acc->usd_balance += converted_amount;
    } else if (strcmp(from_currency, "USD") == 0 && strcmp(to_currency, "GBP") == 0) {
        converted_amount = amount * USD_TO_GBP;
        acc->usd_balance -= amount;
        acc->gbp_balance += converted_amount;
    } else if (strcmp(from_currency, "GBP") == 0 && strcmp(to_currency, "USD") == 0) {
        converted_amount = amount * GBP_TO_USD;
        acc->gbp_balance -= amount;
        acc->usd_balance += converted_amount;
    } else if (strcmp(from_currency, "EUR") == 0 && strcmp(to_currency, "GBP") == 0) {
        converted_amount = amount * EUR_TO_GBP;
        acc->eur_balance -= amount;
        acc->gbp_balance += converted_amount;
    } else if (strcmp(from_currency, "GBP") == 0 && strcmp(to_currency, "EUR") == 0) {
        converted_amount = amount * GBP_TO_EUR;
        acc->gbp_balance -= amount;
        acc->eur_balance += converted_amount;
    }
}

void *client_handler(void *client_socket) {
    int sock = *(int*)client_socket;
    free(client_socket);

    char buffer[BUFFER_SIZE + 1] = {0};
    ssize_t read_size;

    printf("Client connected\n");

    while ((read_size = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0'; 

        char command[50] = {0}, username[50] = {0}, currency1[10] = {0}, currency2[10] = {0};
        double amount = 0.0;

        // Διαβάζουμε πρώτα ΜΟΝΟ την εντολή και το όνομα
        sscanf(buffer, "%49s %49s", command, username);

        pthread_mutex_lock(&account_mutex);

        Account* acc = NULL;
        if (strlen(username) > 0) {
            acc = find_account(username);
        }

        if (strcmp(command, "create") == 0) {
            if (strlen(username) == 0) {
                send(sock, "Error: Username required\n", 25, 0);
            } else if (!acc) {
                create_account(username);
                send(sock, "Account created successfully\n", 30, 0);
            } else {
                send(sock, "Account already exists\n", 24, 0);
            }
        } else if (strcmp(command, "balance") == 0) {
            if (acc) {
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "Balance for %s: USD %.2f, EUR %.2f, GBP %.2f\n",
                         acc->username, acc->usd_balance, acc->eur_balance, acc->gbp_balance);
                send(sock, response, strlen(response), 0);
            } else {
                send(sock, "Account not found\n", 19, 0);
            }
        } else if (strcmp(command, "deposit") == 0) {
            // Διαβάζουμε το ποσό μόνο αν είναι deposit
            if (acc && sscanf(buffer, "%*s %*s %9s %lf", currency1, &amount) == 2) {
                if (strcmp(currency1, "USD") == 0) acc->usd_balance += amount;
                else if (strcmp(currency1, "EUR") == 0) acc->eur_balance += amount;
                else if (strcmp(currency1, "GBP") == 0) acc->gbp_balance += amount;
                else {
                    send(sock, "Invalid currency\n", 18, 0);
                    pthread_mutex_unlock(&account_mutex);
                    continue;
                }
                save_accounts_to_file();
                send(sock, "Deposit successful\n", 20, 0);
            } else send(sock, "Invalid command format\n", 23, 0);
        } else if (strcmp(command, "withdraw") == 0) {
            // Διαβάζουμε το ποσό μόνο αν είναι withdraw
            if (acc && sscanf(buffer, "%*s %*s %9s %lf", currency1, &amount) == 2) {
                if (strcmp(currency1, "USD") == 0 && acc->usd_balance >= amount) acc->usd_balance -= amount;
                else if (strcmp(currency1, "EUR") == 0 && acc->eur_balance >= amount) acc->eur_balance -= amount;
                else if (strcmp(currency1, "GBP") == 0 && acc->gbp_balance >= amount) acc->gbp_balance -= amount;
                else {
                    send(sock, "Insufficient funds or invalid currency\n", 41, 0);
                    pthread_mutex_unlock(&account_mutex);
                    continue;
                }
                save_accounts_to_file();
                send(sock, "Withdrawal successful\n", 23, 0);
            } else send(sock, "Invalid command format\n", 23, 0);
        } else if (strcmp(command, "exchange") == 0) {
            // Διαβάζουμε και τα δύο νομίσματα αν είναι exchange
            if (acc && sscanf(buffer, "%*s %*s %9s %9s %lf", currency1, currency2, &amount) == 3) {
                if (amount <= 0) send(sock, "Invalid amount\n", 15, 0);
                else if (strcmp(currency1, currency2) == 0) send(sock, "Cannot exchange same currency\n", 31, 0);
                else {
                    exchange_currency(acc, currency1, currency2, amount);
                    save_accounts_to_file();
                    send(sock, "Exchange successful\n", 21, 0);
                }
            } else send(sock, "Invalid command format\n", 23, 0);
        } else {
            send(sock, "Invalid command\n", 16, 0);
        }

        pthread_mutex_unlock(&account_mutex);
    }

    close(sock);
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    load_accounts_from_file();

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            continue; 
        }

        int *client_sock = malloc(sizeof(int));
        *client_sock = new_socket;

        if (pthread_create(&thread_id, NULL, client_handler, (void*)client_sock) < 0) {
            free(client_sock);
            close(new_socket);
        }
        pthread_detach(thread_id);
    }
    return 0;
}