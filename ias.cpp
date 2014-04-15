#include "Arduino.h"
#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/proc.h"
#include "mruby/string.h"

mrb_value
mrb_step_x(mrb_state *mrb, mrb_value self)
{
  for (int i = 0; i < 100; i++) {
    digitalWrite(30, HIGH);
    delayMicroseconds(3000);
    digitalWrite(30, LOW);
    delayMicroseconds(3000);
  }
  return mrb_nil_value();
}

mrb_value
mrb_step_y(mrb_state *mrb, mrb_value self)
{
  for (int i = 0; i < 100; i++) {
    digitalWrite(31, HIGH);
    delayMicroseconds(3000);
    digitalWrite(31, LOW);
    delayMicroseconds(3000);
  }
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
  print_hint();

  cxt = mrbc_context_new(mrb);
  cxt->capture_errors = 1;
  ai = mrb_gc_arena_save(mrb);

  krn = mrb->kernel_module;
  mrb_define_method(mrb, krn, "p", my_p, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, krn, "print", my_print, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, krn, "puts", my_puts, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, krn, "step_x", mrb_step_x, MRB_ARGS_NONE());
  mrb_define_method(mrb, krn, "step_y", mrb_step_y, MRB_ARGS_NONE());
}

byte incommingByte;
char last_code_line[1024] = { 0 };
char ruby_code[1024] = { 0 };
int char_index;
int is_exit = false;
int code_block_open = false;

void setup() {
  pinMode(13, OUTPUT);
  pinMode(30, OUTPUT);
  pinMode(31, OUTPUT);
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

    strcpy(ruby_code, last_code_line);

    /* parse code */
    parser = mrb_parser_new(mrb);
    parser->s = ruby_code;
    parser->send = ruby_code + strlen(ruby_code);
    parser->lineno = 1;
    mrb_parser_parse(parser, cxt);

    if (0 < parser->nerr) {
      /* syntax error */
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
      ruby_code[0] = '\0';
      last_code_line[0] = '\0';
      //mrb_gc_arena_restore(mrb, ai);
      mrb_parser_free(parser);
      Serial.write("> ");
    }
  }
}

