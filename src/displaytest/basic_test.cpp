/*#include <Arduino.h>
#include <Wire.h>
#include "displayclass/W32DL1414.h"

W32DL1414 display(0x40);

static void print_status(const char* label)
{
  W32DL1414RawStatus st = display.getStatusRaw();
  Serial.print(label);
  Serial.print(" eng=");
  Serial.print(st.engine_status);
  Serial.print(" op=");
  Serial.print(st.operation_status);
  Serial.print(" err=");
  Serial.print(st.error_code);
  Serial.print(" job=");
  Serial.print(st.job_status);
  Serial.print(" type=");
  Serial.print(st.job_type);
  Serial.print(" off=");
  Serial.print(st.job_offset);
  Serial.print(" prog=");
  Serial.println(st.job_progress);
}

static void section(const char* title)
{
  Serial.println();
  Serial.println(title);
}

static void test_begin_and_baseline()
{
  section("=== baseline ===");
  print_status("after begin:");
  delay(50);
  print_status("after begin +50ms:");
  delay(200);
  print_status("after begin +250ms:");
}

static void test_read_only()
{
  section("=== read only ===");
  print_status("ro before:");

  uint8_t p = display.ping();
  Serial.print("PING: 0x");
  Serial.println(p, HEX);
  print_status("ro after ping:");

  W32DL1414SystemInfo sys = display.getSystemInfo();
  Serial.print("board=");
  Serial.print(sys.board);
  Serial.print(" clock_khz=");
  Serial.print(sys.clock_khz);
  Serial.print(" ram_total=");
  Serial.print(sys.ram_total);
  Serial.print(" ram_free=");
  Serial.println(sys.ram_free);
  print_status("ro after sysinfo:");

  (void)display.getStatusRaw();
  print_status("ro after status:");
}

static void test_write_no_job()
{
  section("=== write no job ===");

  bool ok = display.writeChar(0, 'A');
  Serial.print("writeChar A: ");
  Serial.println(ok ? "OK" : "FAIL");
  print_status("after writeChar A:");

  const uint8_t buf[] = { 'B', 'C', 'D', 'E' };
  ok = display.writeBuffer(4, buf, sizeof(buf));
  Serial.print("writeBuffer BCDE: ");
  Serial.println(ok ? "OK" : "FAIL");
  print_status("after writeBuffer BCDE:");
}

static void test_clear_timeline()
{
  section("=== clear timeline ===");

  print_status("clear before:");
  display.clear(' ');
  print_status("clear t0:");
  delay(20);
  print_status("clear t20:");
  delay(50);
  print_status("clear t70:");
  delay(200);
  print_status("clear t270:");

  bool ok = display.waitUntilIdle(8000);
  Serial.print("clear wait: ");
  Serial.println(ok ? "OK" : "TIMEOUT");
  print_status("clear done:");
}

static void test_flush_timeline()
{
  section("=== flush timeline ===");

  display.print(0, "HALLO");
  print_status("flush before:");

  bool ok = display.flush();
  Serial.print("flush call: ");
  Serial.println(ok ? "OK" : "FAIL");
  print_status("flush after call:");
  delay(20);
  print_status("flush t20:");
  delay(50);
  print_status("flush t70:");
  delay(200);
  print_status("flush t270:");
}

static void test_forced_error_and_recovery()
{
  section("=== provoke error ===");

  bool ok = display.writeChar(W32DL1414_DISPLAY_SIZE, 'X');
  Serial.print("writeChar out_of_range: ");
  Serial.println(ok ? "OK" : "FAIL");
  print_status("after forced error:");

  ok = display.writeChar(0, 'Z');
  Serial.print("writeChar recover: ");
  Serial.println(ok ? "OK" : "FAIL");
  print_status("after recover write:");

  ok = display.flush();
  Serial.print("flush after recover: ");
  Serial.println(ok ? "OK" : "FAIL");
  print_status("after recover flush:");
}

static void test_idle_decay()
{
  section("=== idle decay ===");
  print_status("idle t0:");
  delay(500);
  print_status("idle t500:");
  delay(1000);
  print_status("idle t1500:");
  delay(3000);
  print_status("idle t4500:");
}

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(100000);
  delay(1000);

  Serial.println("=== W32DL1414 master diagnostic ===");

  if (!display.begin()) {
    Serial.println("begin() failed");
    return;
  }

  Serial.print("FW: ");
  Serial.print(display.getFirmwareMajorVersion());
  Serial.print('.');
  Serial.print(display.getFirmwareMinorVersion());
  Serial.print('.');
  Serial.println(display.getFirmwarePatchVersion());

  test_begin_and_baseline();
  test_read_only();
  test_write_no_job();
  test_clear_timeline();
  test_flush_timeline();
  test_forced_error_and_recovery();
  test_idle_decay();

  Serial.println();
  Serial.println("=== diagnostic done ===");
}

void loop()
{
}*/