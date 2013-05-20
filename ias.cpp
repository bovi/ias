#include "Arduino.h"
#include "mruby/include/mruby.h"
#include "mruby/include/mruby/compile.h"
#include "mruby/include/mruby/proc.h"
#include "mruby/include/mruby/string.h"

/* Set pin 13 high and low to blink */
mrb_value
mrb_led_high(mrb_state *mrb, mrb_value self)
{
  digitalWrite(13, HIGH);
  return mrb_nil_value();
}

/* Set pin 13 high and low to blink */
mrb_value
mrb_led_low(mrb_state *mrb, mrb_value self)
{
  digitalWrite(13, LOW);
  return mrb_nil_value();
}

/* Set pin 13 high and low to blink */
mrb_value
mrb_led_blink(mrb_state *mrb, mrb_value self)
{
  digitalWrite(13, HIGH);
  delay(500);
  digitalWrite(13, LOW);
  delay(500);

  return mrb_nil_value();
}

/* Redirecting #p to Serial */
static void
p(mrb_state *mrb, mrb_value obj, int prompt)
{
  obj = mrb_funcall(mrb, obj, "inspect", 0);
  if (prompt) {
    if (!mrb->exc) {
      Serial.write(" => ");
    }
    else {
      obj = mrb_funcall(mrb, mrb_obj_value(mrb->exc), "inspect", 0);
    }
  }
  Serial.println(RSTRING_PTR(obj));
}

mrb_value
my_p(mrb_state *mrb, mrb_value self)
{
  mrb_value argv;

  mrb_get_args(mrb, "o", &argv);
  p(mrb, argv, 0);

  return argv;
}

static void
printstr(mrb_state *mrb, mrb_value obj, int new_line)
{
  mrb_value str;

  str = mrb_funcall(mrb, obj, "to_s", 0);
  if (new_line)
    Serial.println(RSTRING_PTR(str));
  else
    Serial.write(RSTRING_PTR(str));
}

mrb_value
my_print(mrb_state *mrb, mrb_value self)
{
  mrb_value argv;

  mrb_get_args(mrb, "o", &argv);
  printstr(mrb, argv, 0);

  return argv;
}

mrb_value
my_puts(mrb_state *mrb, mrb_value self)
{ 
  mrb_value argv;
  
  mrb_get_args(mrb, "o", &argv);
  printstr(mrb, argv, 1);
  
  return argv;
}

/* Guess if the user might want to enter more
 * or if he wants an evaluation of his code now */
int
is_code_block_open(struct mrb_parser_state *parser)
{
  int code_block_open = FALSE;

  /* check for heredoc */
  if (parser->parsing_heredoc != NULL) return TRUE;
  if (parser->heredoc_end_now) {
    parser->heredoc_end_now = FALSE;
    return FALSE;
  }

  /* check if parser error are available */
  if (0 < parser->nerr) {
    const char *unexpected_end = "syntax error, unexpected $end";
    const char *message = parser->error_buffer[0].message;

    /* a parser error occur, we have to check if */
    /* we need to read one more line or if there is */
    /* a different issue which we have to show to */
    /* the user */

    if (strncmp(message, unexpected_end, strlen(unexpected_end)) == 0) {
      code_block_open = TRUE;
    }
    else if (strcmp(message, "syntax error, unexpected keyword_end") == 0) {
      code_block_open = FALSE;
    }
    else if (strcmp(message, "syntax error, unexpected tREGEXP_BEG") == 0) {
      code_block_open = FALSE;
    }
    return code_block_open;
  }

  /* check for unterminated string */
  if (parser->lex_strterm) return TRUE;

  switch (parser->lstate) {

  /* all states which need more code */

  case EXPR_BEG:
    /* an expression was just started, */
    /* we can't end it like this */
    code_block_open = TRUE;
    break;
  case EXPR_DOT:
    /* a message dot was the last token, */
    /* there has to come more */
    code_block_open = TRUE;
    break;
  case EXPR_CLASS:
    /* a class keyword is not enough! */
    /* we need also a name of the class */
    code_block_open = TRUE;
    break;
  case EXPR_FNAME:
    /* a method name is necessary */
    code_block_open = TRUE;
    break;
  case EXPR_VALUE:
    /* if, elsif, etc. without condition */
    code_block_open = TRUE;
    break;

  /* now all the states which are closed */

  case EXPR_ARG:
    /* an argument is the last token */
    code_block_open = FALSE;
    break;

  /* all states which are unsure */

  case EXPR_CMDARG:
    break;
  case EXPR_END:
    /* an expression was ended */
    break;
  case EXPR_ENDARG:
    /* closing parenthese */
    break;
  case EXPR_ENDFN:
    /* definition end */
    break;
  case EXPR_MID:
    /* jump keyword like break, return, ... */
    break;
  case EXPR_MAX_STATE:
    /* don't know what to do with this token */
    break;
  default:
    /* this state is unexpected! */
    break;
  }

  return code_block_open;
}

/* Print the command line prompt of the REPL */
void
print_cmdline(int code_block_open)
{
  if (code_block_open) {
    Serial.write("* ");
  } else {
    Serial.write("> ");
  }
}

/* Print a short remark for the user */
static void
print_hint(void)
{
  Serial.println("");
  Serial.println("");
  Serial.println("");
  Serial.println("IAS - Interactive Arduino Shell");
  Serial.println("");
  Serial.println("This is a very early version, please test and report errors.");
  Serial.println("Thanks :)");
  Serial.println("");
}

mrb_state *mrb;
mrbc_context *cxt;
int ai;
struct mrb_parser_state *parser;
int n;
mrb_value result;
struct RClass *krn;
struct RClass *led;

void
mrb_setup_arduino() {
  mrb = mrb_open();
  if (mrb == NULL)
    Serial.println("Invalid mrb interpreter, IAS won't work!");
  else
    print_hint();

  cxt = mrbc_context_new(mrb);
  cxt->capture_errors = 1;
  ai = mrb_gc_arena_save(mrb);

  krn = mrb->kernel_module;
  mrb_define_method(mrb, krn, "p", my_p, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, krn, "print", my_print, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, krn, "puts", my_puts, MRB_ARGS_REQ(1));

  led = mrb_define_class(mrb, "Led", mrb->object_class);
  mrb_define_class_method(mrb, led, "high!", mrb_led_high, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, led, "low!", mrb_led_low, MRB_ARGS_NONE());
  mrb_define_class_method(mrb, led, "blink", mrb_led_blink, MRB_ARGS_NONE());
}

byte incommingByte;
char last_code_line[1024] = { 0 };
char ruby_code[1024] = { 0 };
int char_index;
int is_exit = false;
int code_block_open = false;

void setup() {
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  Serial.begin(9600);

  mrb_setup_arduino();

  Serial.write("> ");
}

void loop() {
  if (Serial.available() > 0) {
    char_index = 0;
    while (true) {
      if (Serial.available() > 0) {
        incommingByte = Serial.read();
        if (incommingByte == 13) {
          // New Line
          last_code_line[char_index] = '\0';
          break;
        } else {
          last_code_line[char_index++] = incommingByte;
          Serial.write(incommingByte);
        }
      }
    }
    Serial.println("");
    Serial.flush();

    is_exit = false;
    if ((strcmp(last_code_line, "quit") == 0) || (strcmp(last_code_line, "exit") == 0)) {
      // Exit Interactive Mode
      if (!code_block_open) {
        is_exit = true;
      } else {
        /* count the quit/exit commands as strings if in a quote block */
        strcat(ruby_code, "\n");
        strcat(ruby_code, last_code_line);
        is_exit = false;
      }
    } else {
      if (code_block_open) {
        strcat(ruby_code, "\n");
        strcat(ruby_code, last_code_line);
        is_exit = false;
      } else {
        strcpy(ruby_code, last_code_line);
        is_exit = false;
      }
    }

    if (!is_exit) {
      // Not Exit or Quit!

      /* parse code */
      parser = mrb_parser_new(mrb);
      parser->s = ruby_code;
      parser->send = ruby_code + strlen(ruby_code);
      parser->lineno = 1;
      mrb_parser_parse(parser, cxt);
      code_block_open = is_code_block_open(parser);

      if (code_block_open) {
        /* no evaluation of code */
      }
      else {
        if (0 < parser->nerr) {
          /* syntax error */
          //printf("line %d: %s\n", parser->error_buffer[0].lineno, parser->error_buffer[0].message);
          Serial.write("Syntax Error: ");
          Serial.println(parser->error_buffer[0].message);
        }
        else {
          /* generate bytecode */
          n = mrb_generate_code(mrb, parser);

          /* evaluate the bytecode */
          result = mrb_run(mrb,
              /* pass a proc for evaulation */
              mrb_proc_new(mrb, mrb->irep[n]),
              mrb_top_self(mrb));

          /* did an exception occur? */
          if (mrb->exc) {
            /* yes */
            p(mrb, mrb_obj_value(mrb->exc), 0);
            mrb->exc = 0;
          }
          else {
            /* no */
            if (!mrb_respond_to(mrb,result,mrb_intern(mrb, "inspect"))){
              result = mrb_any_to_s(mrb,result);
            }
            p(mrb, result, 1);
          }
        }
        ruby_code[0] = '\0';
        last_code_line[0] = '\0';
        mrb_gc_arena_restore(mrb, ai);
      }
      mrb_parser_free(parser);
    } else {
      // User Enter 'exit' or 'quit'
      Serial.println("quit IAS");
      ruby_code[0] = '\0';
      last_code_line[0] = '\0';
      mrb_parser_free(parser);
      mrbc_context_free(mrb, cxt);
      mrb_close(mrb);
      mrb_setup_arduino();
    }
    print_cmdline(code_block_open);
  }
}

