#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <yaml.h>
#include <sys/types.h>
#include <pwd.h>
#include "rk_parse.h"

static void parse_ls(int argc, char *argv[], action_t *action);
static void parse_add(int argc, char *argv[], action_t *action);
static void parse_cat(int argc, char *argv[], action_t *action);
static void parse_rm(int argc, char *argv[], action_t *action);
static void parse_prop(int argc, char *argv[], action_t *action);
static void parse_map(int argc, char *argv[], action_t *action);
static void parse_grep(int argc, char *argv[], action_t *action);
static void emit_help(int argc, char *argv[], action_t *action);

#define SDEF(E, T) [(E)] = (T)
static char *subcommands[] = { /* follows order of subcommand_t */
  SDEF(RK_SC_LS, "ls"),
  SDEF(RK_SC_ADD, "add"),
  SDEF(RK_SC_CAT, "cat"),
  SDEF(RK_SC_RM, "rm"),
  SDEF(RK_SC_PROP, "prop"),
  SDEF(RK_SC_MAP, "map"),
  SDEF(RK_SC_GREP, "grep"),
  SDEF(RK_SC_HELP, "help"),
  SDEF(RK_SC_MAX, NULL)
};

static void (*subcmd_funcs[])(int, char *[], action_t *) = {
  SDEF(RK_SC_LS, parse_ls),
  SDEF(RK_SC_ADD, parse_add),
  SDEF(RK_SC_CAT, parse_cat),
  SDEF(RK_SC_RM, parse_rm),
  SDEF(RK_SC_PROP, parse_prop),
  SDEF(RK_SC_MAP, parse_map),
  SDEF(RK_SC_GREP, parse_grep),
  SDEF(RK_SC_HELP, emit_help),
  SDEF(RK_SC_MAX, emit_help)
};
#undef SDEF

char *
file2str(FILE *f, size_t *len)
{
  char *s;
  int i;

  s = malloc(sizeof(char) * 1024);
  *len = 0;
  while(1) {
    i = fread(s + *len, 1, 1024, f);
    *len += i;
    if (i != 1024) {
      return s;
    } else {
      s = realloc(s, *len + 1024);
      if (!s) {
        return NULL;
      }
    }
  }
}

void
dump_action(action_t *action)
{
  int i;

  printf("n_urls: %d\n", action->n_urls);
  for (i = 0; i < action->n_urls; i++) {
    printf("host:port[%d] = '%s:%s'\n", i, action->hosts[i], action->ports[i]);
  }
  printf("subcommand: %s\n", subcommands[action->subcommand]);
  switch (action->subcommand) {
    case RK_SC_CAT:
      printf("  cat.human: %d\n", action->cat.human);
      printf("  cat.number: %d\n", action->cat.number);
      printf("  cat.bucket: %s\n", action->cat.bucket);
      printf("  cat.key: %.*s\n", (int)action->cat.key.len,
          action->cat.key.data);
      break;
    case RK_SC_LS:
      printf("  ls.verbose: %d\n", action->ls.verbose);
      printf("  ls.buckets: %s", action->ls.n_buckets? "{": "<none>\n");
      for (i = 0; i < action->ls.n_buckets; i++) {
        printf("'%s'%s", action->ls.buckets[i],
            action->ls.n_buckets - i == 1? "}\n": ", ");
      }
      break;
    case RK_SC_RM:
      printf("  rm.bucket: %s\n", action->rm.bucket);
      printf("  rm.key: %.*s\n", (int)action->rm.key.len, action->rm.key.data);
      break;
    case RK_SC_MAP:
      printf("  map.expr_type: %s\n", action->map.expr_type);
      printf("  map.expression: '%s'\n", action->map.expression);
      break;
    case RK_SC_GREP:
      printf("  grep.just_keys: %d\n", action->grep.just_keys);
      printf("  grep.regex: %s\n", action->grep.regex);
      printf("  grep.bucket: %s\n", action->grep.bucket);
      printf("  grep.keys: %s", action->grep.n_keys? "{": "<none>\n");
      for (i = 0; i < action->grep.n_keys; i++) {
        printf("'%s'%s", action->grep.keys[i],
            action->grep.n_keys - i == 1? "}\n": ", ");
      }
      break;
    default:
      printf("Don't know how to print '%s' subcommands.\n",
          subcommands[action->subcommand]);
      break;
  }
}

void
usage(char *message)
{
  char *u[] = {
    "USAGE: rk [-v] [-s host:port] <command>",
    "    -v - Verbose.",
    "    -s - Add the given server. Can be specified multiple times.",
    "    Where command is one of:",
    "    ls   - [bucket1 bucket2 ...]",
    "           List buckets (no buckets given). List keys in buckets.",
    "           -l - verbose",
    "    add  - <bucket> <key>",
    "           Add value from stdin to key in bucket.",
    "           -f - file to get value from.",
    "    rm   - <bucket> <key1> [key2 ...]",
    "           Remove keys from bucket.",
    "    prop - TODO <bucket>",
    "           Set properties on a bucket.",
    "    map  - [-t <js|erl|type>] [-e expresion] [-f expression file]",
    "             <bucket1> [bucket2 ...]",
    "           -t - type of expression, js and erl are shortcuts.",
    "           -e - Code to run over all the keys.",
    "           -f - File to load the code from.",
    "           Run expression from -e or -f over all the keys in the",
    "           given buckets.",
    "           Search keys in given buckets for a pattern.",
    "    grep - <pattern> <bucket> [key1 key2 ... keyn]",
    "           Search all keys (or listed keys) in bucket for a pattern.",
    "    help - This.",
    "Environment:",
    "    RK_SERVERS - Semi-colon delimited list of servers used in",
    "                 addition to the global -s flag.",
    "                 <host:port[;host:port;...]>",
    NULL
  };
  int i;

  if (message) {
    printf("ERROR: %s\n", message);
  }
  for (i = 0; u[i]; i++) {
    printf("%s\n", u[i]);
  }
  exit(message? 1: 0);
}

static void
emit_help(int argc, char *argv[], action_t *action)
{
  if (action->subcommand == RK_SC_HELP) {
    usage(NULL);
  } else {
    usage("Unknown command.");
  }
}

#define CONF_FILE ".rk.conf"
#define CONF_ENV_VAR "RK_CONFIG"

typedef enum {
  CF_START,
  CF_VAR_UNKNOWN,
  CF_VAR_SERVERS,
} cf_state_t;

static void
parse_config_file(action_t *action)
{
  yaml_parser_t parser;
  yaml_event_t event;
  char *cfg_fn, *home = NULL;
  int cfg_fn_len, done = 0;
  FILE *cfg;
  cf_state_t cf_state = CF_START;

  if (getenv(CONF_ENV_VAR) == NULL) {
    if (getenv("HOME") == NULL) {
      struct passwd *pw;

      pw = getpwuid(getuid());
      if (pw) {
        home = strdup(pw->pw_dir);
      }
    } else {
      home = strdup(getenv("HOME"));
    }
    if (!home) {
      return;
    }
    cfg_fn_len = strlen(home) + strlen(CONF_FILE) + 2;
    cfg_fn = malloc(cfg_fn_len);
    snprintf(cfg_fn, cfg_fn_len, "%s/%s", home, CONF_FILE);
    free(home);
  } else {
    cfg_fn = strdup(getenv(CONF_ENV_VAR));
  }

  cfg = fopen(cfg_fn, "r");
  if (!cfg) {
    free(cfg_fn);
    return;
  }

  yaml_parser_initialize(&parser);
  yaml_parser_set_input_file(&parser, cfg);

  while (!done) {
    if (!yaml_parser_parse(&parser, &event)) {
      break;
    }

    switch (event.type) {
      case YAML_NO_EVENT:
      case YAML_STREAM_START_EVENT:
      case YAML_STREAM_END_EVENT:
      case YAML_DOCUMENT_START_EVENT:
      case YAML_DOCUMENT_END_EVENT:
      case YAML_SEQUENCE_START_EVENT:
      case YAML_SEQUENCE_END_EVENT:
      case YAML_MAPPING_END_EVENT:
      case YAML_ALIAS_EVENT:
        /* Not really interesting.
         * TODO: Get interested - should only parse things at
         * global level. */
        break;
      case YAML_MAPPING_START_EVENT:
        cf_state = CF_VAR_UNKNOWN;
        break;
      case YAML_SCALAR_EVENT:
        if (cf_state == CF_VAR_UNKNOWN) {
          if (strcmp("servers", event.data.scalar.value) == 0) {
            cf_state = CF_VAR_SERVERS;
          } else {
            usage("ERROR: Unknown config file var.");
          }
        } else {
          if (cf_state == CF_VAR_SERVERS) {
            action->hosts = realloc(action->hosts,
                sizeof(char *) * (action->n_urls + 1));
            action->ports = realloc(action->ports,
                sizeof(char *) * (action->n_urls + 1));
            if (!action->hosts || !action->ports) {
              usage("ERROR: Out of memory parsing environment.");
            } else {
              action->hosts[action->n_urls] = strdup(event.data.scalar.value);
              action->ports[action->n_urls]
                = strchr(action->hosts[action->n_urls], ':');
              if (!action->ports[action->n_urls]) {
                usage("ERROR: Server missing : delim between host and port.");
              }
              action->ports[action->n_urls][0] = '\0';
              action->ports[action->n_urls]++;
              action->n_urls++;
            }
          } else {
            usage("ERROR: Config file parsing error.");
          }
        }
        break;
      default:
        usage("ERROR: Config file parsing error.");
        break;
    }

    done = (event.type == YAML_STREAM_END_EVENT);
    yaml_event_delete(&event);
  }

  yaml_parser_delete(&parser);
  free(cfg_fn);
  fclose(cfg);
}

static void
parse_environment(action_t *action)
{
  char *urls, *walk;
  int i, len;

  urls = getenv("RK_SERVERS");
  if (!urls) {
    return;
  }
  i = 0;
  while (urls[i]) {
    len = i;
    while (urls[len] && urls[len] != ';') {
      len++;
    }
    if (len - i == 0) {
      break;
    }
    action->hosts = realloc(action->hosts,
        sizeof(char *) * (action->n_urls + 1));
    action->ports = realloc(action->ports,
        sizeof(char *) * (action->n_urls + 1));
    if (!action->hosts || !action->ports) {
      usage("ERROR: Out of memory parsing environment.");
    } else {
      action->hosts[action->n_urls] = malloc(len - i + 1);
      if (!action->hosts[action->n_urls]) {
        usage("ERROR: Out of memory parsing environment.");
      }
      strncpy(action->hosts[action->n_urls], urls + i, len - i);
      action->hosts[action->n_urls][len - i] = '\0';
      action->ports[action->n_urls]
        = strchr(action->hosts[action->n_urls], ':');
      if (!action->ports[action->n_urls]) {
        usage("ERROR: Server missing : delim between host and port.");
      }
      action->ports[action->n_urls][0] = '\0';
      action->ports[action->n_urls]++;
      action->n_urls++;
      if (!urls[len]) {
        break;
      }
    }
  }
}

static void
parse_ls(int argc, char *argv[], action_t *action)
{
  int c, i;
  struct option options[] = {
    {"long",    no_argument,       0,  'l' },
    {"hex",     no_argument,       0,  'x' },
    {0,         0,                 0,  0   }
  };

  action->ls.verbose = 0;
  action->ls.hex = 0;
  while (1) {
    c = getopt_long(argc, argv, "lx", options, NULL);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 'l':
        action->ls.verbose = 1;
        break;
      case 'x':
        action->ls.hex = 1;
        break;
      default:
        usage("ls: Unknown option.");
    }
  }
  action->ls.n_buckets = argc - optind;
  if (action->ls.n_buckets <= 0) {
    action->ls.n_buckets = 0;
    action->ls.buckets = NULL;
  } else {
    action->ls.buckets = malloc(sizeof(char *) * action->ls.n_buckets);
    for (i = 0; i < action->ls.n_buckets; i++) {
      action->ls.buckets[i] = strdup(argv[optind++]);
    }
  }
}

static void
parse_add(int argc, char *argv[], action_t *action)
{
  int c, hex = 0;
  struct option options[] = {
    {"file",    required_argument, 0,  'f' },
    {"hex",     no_argument,       0,  'x' },
    {0,         0,                 0,  0   }
  };
  FILE *value_f;

  action->add.filename = NULL;
  while (1) {
    c = getopt_long(argc, argv, "f:x", options, NULL);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 'f':
        if (action->add.filename) {
          usage("add: Only one -f arg is allowed.");
        }
        action->add.filename = strdup(optarg);
        break;
      case 'x':
        hex = 1;
        break;
      default:
        usage("add: Unknown option.");
    }
  }
  if (argc - optind != 2) {
    usage("add: must supply a bucket and a key.");
  }
  action->add.bucket = strdup(argv[optind]);
  optind++;
  if (hex) {
    int len, i;

    len = strlen(argv[optind]);
    if (len % 2) {
      usage("cat: key must be an even number of hex chars long.");
    }
    action->add.key.len = len / 2;
    action->add.key.data = malloc(sizeof(char) * (len /2));
    for (i = 0; i < (len / 2); i++) {
      sscanf(argv[optind] + (i * 2), "%2hhx", action->add.key.data + i);
    }
  } else {
    action->add.key.len = strlen(argv[optind]);
    action->add.key.data = strdup(argv[optind]);
  }
  if (action->add.filename) {
    value_f = fopen(action->add.filename, "r");
    if (!value_f) {
      usage("add: Couldn't open value file.");
    }
  } else {
    value_f = stdin;
  }
  action->add.value.data = file2str(value_f, &action->add.value.len);
  action->add.value.data = malloc(1024);
  if (!action->add.value.data) {
    usage("add: Ran out of memory reading in value file.");
  }
}

static void
parse_cat(int argc, char *argv[], action_t *action)
{
  int c, hex = 0;
  struct option options[] = {
    {"human",   no_argument,       0,  'h' },
    {"number",  no_argument,       0,  'n' },
    {"hex",     no_argument,       0,  'x' },
    {0,         0,                 0,  0   }
  };

  action->cat.human = 0;
  action->cat.number = 0;
  while (1) {
    c = getopt_long(argc, argv, "hnx", options, NULL);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 'h':
        action->cat.human = 1;
        break;
      case 'n':
        action->cat.number = 1;
        break;
      case 'x':
        hex = 1;
        break;
      default:
        usage("cat: Unknown option.");
    }
  }
  if (argc - optind != 2) {
    usage("cat: must supply a bucket and a key.");
  }
  action->cat.bucket = strdup(argv[optind]);
  optind++;
  if (hex) {
    int i;
    int len = strlen(argv[optind]);

    if (len % 2) {
      usage("cat: key must be an even number of hex chars long.");
    }
    action->cat.key.len = len / 2;
    action->cat.key.data = malloc(sizeof(char) * (len /2));
    for (i = 0; i < (len / 2); i++) {
      sscanf(argv[optind] + (i * 2), "%2hhx", action->cat.key.data + i);
    }
  } else {
    action->cat.key.len = strlen(argv[optind]);
    action->cat.key.data = strdup(argv[optind]);
  }
  optind++;
}

static void
parse_rm(int argc, char *argv[], action_t *action)
{
  int c, hex = 0;
  struct option options[] = {
    {"hex",       no_argument,       0,  'x' },
    {"recursive", no_argument,       0,  'r' },
    {"force",     no_argument,       0,  'f' },
    {0,           0,                 0,  0   }
  };

  action->rm.recursive = 0;
  action->rm.force = 0;
  while (1) {
    c = getopt_long(argc, argv, "hxrf", options, NULL);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 'x':
        hex = 1;
        break;
      case 'r':
        action->rm.recursive = 1;
        break;
      case 'f':
        action->rm.force = 1;
        break;
      default:
        usage("cat: Unknown option.");
    }
  }
  if (action->rm.recursive && action->rm.force) {
    if (argc - optind < 1 || argc - optind > 2) {
      usage("rm: must supply bucket or bucket and key.");
    }
  } else {
    if (argc - optind != 2) {
      usage("rm: must supply bucket and key.");
    }
  }
  action->rm.bucket = strdup(argv[optind++]);
  if (argc != optind) {
    if (hex) {
      int i;
      int len = strlen(argv[optind]);

      if (len % 2) {
        usage("rm: key must be an even number of hex chars long.");
      }
      action->rm.key.len = len / 2;
      action->rm.key.data = malloc(sizeof(char) * (len /2));
      for (i = 0; i < (len / 2); i++) {
        sscanf(argv[optind] + (i * 2), "%2hhx", action->rm.key.data + i);
      }
    } else {
      action->rm.key.len = strlen(argv[optind]);
      action->rm.key.data = strdup(argv[optind]);
    }
    optind++;
  } else {
    action->rm.key.len = 0;
    action->rm.key.data = NULL;
  }
}

static void
parse_prop(int argc, char *argv[], action_t *action)
{
}

static void
parse_map(int argc, char *argv[], action_t *action)
{
  int c, hex = 0;
  struct option options[] = {
    {"type",    required_argument, 0,  't' },
    {"expr",    required_argument, 0,  'e' },
    {"exprfile",required_argument, 0,  'f' },
    {0,         0,                 0,  0   }
  };
  size_t len;
  FILE *expr_f;

  action->map.expr_type = NULL;
  action->map.expression = NULL;
  while (1) {
    c = getopt_long(argc, argv, "t:e:f:", options, NULL);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 't':
        if (action->map.expr_type) {
          usage("map: Only specify the expression type once.");
        }
        if (strcmp(optarg, "js") == 0) {
          action->map.expr_type = strdup("application/json");
        }
        else if (strcmp(optarg, "erl") == 0) {
          action->map.expr_type = strdup("application/x-erlang-binary");
        } else {
          action->map.expr_type = strdup(optarg);
        }
        break;
      case 'e':
        if (action->map.expression) {
          usage("map: Only specify the expression once.");
        }
        action->map.expression = strdup(optarg);
        if (!action->map.expression) {
          usage("map: Failed to allocate memory for expression.");
        }
        break;
      case 'f':
        expr_f = fopen(optarg, "r");
        action->map.expression = file2str(expr_f, &len);
        if (!action->map.expression) {
          usage("map: Failed to allocate memory for expression.");
        }
        break;
      default:
        usage("map: Unknown option.");
    }
  }
  if (!action->map.expr_type) {
    usage("map: An expression type must be given with the -t flag.");
  }
  if (!action->map.expression) {
    usage("map: An expression must be given with the -e or -f flags.");
  }

  if (argc - optind > 0) {
    usage("map: Unknown arguments.");
  }
}

static void
parse_grep(int argc, char *argv[], action_t *action)
{
  int c, hex = 0;
  struct option options[] = {
    {"files-with-matches", no_argument, 0,  'l' },
    {"regexp",       required_argument, 0,  'e' },
    {0, 0, 0, 0}
  };
  size_t len;
  FILE *expr_f;

  action->grep.regex = NULL;
  action->grep.just_keys = 0;
  while (1) {
    c = getopt_long(argc, argv, "le:", options, NULL);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 'e':
        if (action->grep.regex) {
          usage("grep: Only specify the regex once.");
        }
        action->grep.regex = strdup(optarg);
        if (!action->grep.regex) {
          usage("grep: Failed to allocate memory for regex.");
        }
        break;
      case 'l':
        action->grep.just_keys = 1;
        break;
      default:
        usage("grep: Unknown option.");
    }
  }
  if (!action->grep.regex) {
    if (argc - optind < 1) {
      usage("grep: An regex must be given.");
    }
    action->grep.regex = strdup(argv[optind++]);
  }

  if (argc - optind < 1) {
    usage("grep: must supply a bucket.");
  }
  action->grep.bucket = strdup(argv[optind++]);
  action->grep.n_keys = argc - optind;
  if (action->grep.n_keys <= 0) {
    action->grep.n_keys = 0;
    action->grep.keys = NULL;
  } else {
    int i;

    action->grep.keys = malloc(sizeof(char *) * action->grep.n_keys);
    for (i = 0; i < action->grep.n_keys; i++) {
      action->grep.keys[i] = strdup(argv[optind++]);
    }
  }
}

action_t *
parse_commandline(int argc, char **argv)
{
  int c, i;
  action_t *action;
  struct option options[] = {
    {"server",  required_argument, 0,  's' },
    {"verbose", no_argument,       0,  'v' },
    {"debug",   no_argument,       0,  'd' },
    {"help",    no_argument,       0,  'h' },
    {0,         0,                 0,  0   }
  };

  if (!(action = malloc(sizeof(action_t)))) {
    return NULL;
  }
  action->n_urls = 0;
  action->hosts = action->ports = NULL;
  action->verbose = 0;
  action->debug = 0;

  parse_config_file(action);
  parse_environment(action);

  while (1) {
    if (!argv[optind] || argv[optind][0] != '-') {
      break;
    }

    c = getopt_long(argc, argv, "s:vdh", options, NULL);
    if (c == -1) {
      break;
    }
    switch (c) {
      case 's':
        i = action->n_urls;
        action->n_urls++;
        action->hosts = realloc(action->hosts, sizeof(char *) * action->n_urls);
        action->ports = realloc(action->ports, sizeof(char *) * action->n_urls);
        if (strchr(optarg, ':')) {
          action->hosts[i] = strdup(optarg);
          action->ports[i] = strchr(action->hosts[i], ':');
          action->ports[i][0] = '\0';
          action->ports[i]++;
        } else {
          usage("Unknown url.");
        }
        break;
      case 'v':
        action->verbose = 1;
        break;
      case 'd':
        action->debug = 1;
        break;
      case '?':
      case 'h':
        usage(NULL);
        break;
      default:
        usage("Unknown option.");
    }
  }

  if (argv[optind]) {
    action->subcommand = 0;
    while (subcommands[action->subcommand]
        && (strcmp(argv[optind], subcommands[action->subcommand]) != 0)) {
      action->subcommand++;
    }
    optind++;
  } else {
    action->subcommand = RK_SC_HELP;
  }

  if (subcmd_funcs[action->subcommand]) {
    subcmd_funcs[action->subcommand](argc, argv, action);
  }

  return action;
}
