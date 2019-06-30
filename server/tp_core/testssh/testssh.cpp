// testssh.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <libssh/libssh.h>

#ifdef _DEBUG
#	pragma comment(lib, "debug/ssh.lib")
#else
#	pragma comment(lib, "release/ssh.lib")
#endif
#pragma comment(lib, "libeay32.lib")
#pragma comment(lib, "ws2_32.lib")


void show_usage() {
    printf("Usage:\n");
	printf("  testssh USERNAME PASSWORD IP PORT\n");
}

int main(int argc, char** argv)
{
	if (argc != 5) {
		show_usage();
		return -1;
	}

    ssh_init();

	ssh_session sess = ssh_new();
	ssh_set_blocking(sess, 1);

    char* username = argv[1];
    char* password = argv[2];

	char* ip = argv[3];
	ssh_options_set(sess, SSH_OPTIONS_HOST, ip);

	int port = atoi(argv[4]);
	ssh_options_set(sess, SSH_OPTIONS_PORT, &port);

	int flag = SSH_LOG_FUNCTIONS;
	ssh_options_set(sess, SSH_OPTIONS_LOG_VERBOSITY, &flag);

    int val = 0;
    ssh_options_set(sess, SSH_OPTIONS_STRICTHOSTKEYCHECK, &val);

	ssh_options_set(sess, SSH_OPTIONS_USER, username);

	int _timeout = 60; // 60 sec.
	ssh_options_set(sess, SSH_OPTIONS_TIMEOUT, &_timeout);

    // connect to real SSH host.
	int rc = 0;
	rc = ssh_connect(sess);
	if (rc != SSH_OK) {
		printf("[ERROR] can not connect to SSH server %s:%d. [%d] %s\n", ip, port, rc, ssh_get_error(sess));
		ssh_free(sess);
		return -1;
	}

    _timeout = 10; // 60 sec.
    ssh_options_set(sess, SSH_OPTIONS_TIMEOUT, &_timeout);

    // get version of SSH server.
    int ver = ssh_get_version(sess);
    printf("[INFO] host is SSHv%d\n", ver);

    // get supported auth-type of SSH server.
    //ssh_userauth_none(sess, username);
    rc = ssh_userauth_none(sess, NULL);
    if (rc == SSH_AUTH_ERROR) {
        printf("[ERROR] can not got auth type supported by SSH server.\n");
        ssh_free(sess);
        return -1;
    }

    int auth_methods = ssh_userauth_list(sess, username);
    printf("[INFO] supported auth-type: 0x%08x\n", auth_methods);

    // get banner.
    const char* banner = ssh_get_issue_banner(sess);
    if (banner != nullptr) {
        printf("[INFO] server issue banner: %s\n", banner);
    }

    // try auth.
    bool ok = false;
    int retry_count = 0;

    // first try interactive login mode if server allow.
    if (!ok && (auth_methods & SSH_AUTH_METHOD_INTERACTIVE) == SSH_AUTH_METHOD_INTERACTIVE) {
        retry_count = 0;
        rc = ssh_userauth_kbdint(sess, nullptr, nullptr);
        for (;;) {
            if (rc == SSH_AUTH_SUCCESS) {
                ok = true;
                break;
            }

            if (rc == SSH_AUTH_AGAIN) {
                retry_count += 1;
                if (retry_count >= 5)
                    break;
                //ex_sleep_ms(500);
                Sleep(500);
                rc = ssh_userauth_kbdint(sess, nullptr, nullptr);
                continue;
            }

            if (rc != SSH_AUTH_INFO)
                break;

            int nprompts = ssh_userauth_kbdint_getnprompts(sess);
            if (0 == nprompts) {
                rc = ssh_userauth_kbdint(sess, nullptr, nullptr);
                continue;
            }

            for (int iprompt = 0; iprompt < nprompts; ++iprompt) {
                char echo = 0;
                const char *prompt = ssh_userauth_kbdint_getprompt(sess, iprompt, &echo);
                printf("[INFO] interactive login prompt: %s\n", prompt);

                rc = ssh_userauth_kbdint_setanswer(sess, iprompt, password);
                if (rc < 0) {
                    printf("[ERROR] invalid password for interactive mode to login to SSH server.\n");
                    ssh_free(sess);
                    return -1;
                }
            }

            rc = ssh_userauth_kbdint(sess, nullptr, nullptr);
        }
    }

    // and then try password login mode if server allow.
    if (!ok && (auth_methods & SSH_AUTH_METHOD_PASSWORD) == SSH_AUTH_METHOD_PASSWORD) {
        retry_count = 0;
        rc = ssh_userauth_password(sess, nullptr, password);
        for (;;) {
            if (rc == SSH_AUTH_AGAIN) {
                retry_count += 1;
                if (retry_count >= 3)
                    break;
                Sleep(100);
                rc = ssh_userauth_password(sess, nullptr, password);
                continue;
            }
            if (rc == SSH_AUTH_SUCCESS) {
                ok = true;
                printf("[INFO] login with password mode OK.\n");
                break;
            } else {
                printf("[ERROR] failed to login with password mode, got %d.\n", rc);
                break;
            }
        }
    }

    if (!ok) {
        printf("[ERROR] can not use password mode or interactive mode to login to SSH server.\n");
    }

    ssh_disconnect(sess);
	ssh_free(sess);
	return 0;
}
