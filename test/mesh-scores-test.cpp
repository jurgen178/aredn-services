#include <cassert>
#include <cstdio>
#include <cstring>
#include "../MeshScores.h"

int main()
{
  Preferences::data.clear();
  MeshScores scores;
  assert(scores.begin());
  assert(scores.submit("Alice", 100) == MeshScores::Result::Ok);
  assert(scores.submit("bob", 200) == MeshScores::Result::Ok);
  assert(scores.submit("ALICE", 90) == MeshScores::Result::Ok);
  MeshScores::List list{};
  assert(scores.read(list) == MeshScores::Result::Ok);
  assert(list.count == 2 && std::strcmp(list.entries[0].name, "bob") == 0);
  assert(list.entries[1].score == 100);
  assert(scores.submit("alice", 300) == MeshScores::Result::Ok);
  assert(scores.read(list) == MeshScores::Result::Ok);
  assert(list.count == 2 && list.entries[0].score == 300);
  assert(scores.submit("<bad>", 1) == MeshScores::Result::Invalid);
  for (int i = 0; i < 12; ++i)
  {
    char name[16];
    std::snprintf(name, sizeof(name), "P%d", i);
    assert(scores.submit(name, 1000 + i) == MeshScores::Result::Ok);
  }
  assert(scores.read(list) == MeshScores::Result::Ok);
  assert(list.count == 10 && list.entries[0].score == 1011 && list.entries[9].score == 1002);
  Preferences::failWrites = true;
  assert(scores.submit("P11", 1000) == MeshScores::Result::Ok);
  assert(scores.submit("TOOLOW", 1) == MeshScores::Result::Ok);
  Preferences::failWrites = false;
  MeshScores rebooted;
  assert(rebooted.begin());
  MeshScores::List restored{};
  assert(rebooted.read(restored) == MeshScores::Result::Ok);
  assert(std::memcmp(&list, &restored, sizeof(list)) == 0);
  MeshScores::List invalid = list;
  invalid.entries[1] = invalid.entries[0];
  assert(rebooted.replace(invalid) == MeshScores::Result::Invalid);
  assert(rebooted.read(restored) == MeshScores::Result::Ok);
  assert(std::memcmp(&list, &restored, sizeof(list)) == 0);
  Preferences::failWrites = true;
  assert(rebooted.submit("NEW", 9999) == MeshScores::Result::Unavailable);
  assert(rebooted.read(restored) == MeshScores::Result::Ok);
  assert(std::memcmp(&list, &restored, sizeof(list)) == 0);
  Preferences::failWrites = false;
  assert(rebooted.replace(list) == MeshScores::Result::Ok);
  MeshScores afterRestore;
  assert(afterRestore.begin());
  assert(afterRestore.read(restored) == MeshScores::Result::Ok);
  assert(std::memcmp(&list, &restored, sizeof(list)) == 0);
  MeshScores::List empty{};
  empty.version = 1;
  assert(rebooted.replace(empty) == MeshScores::Result::Ok);
  MeshScores afterClear;
  assert(afterClear.begin());
  assert(afterClear.read(restored) == MeshScores::Result::Ok && restored.count == 0);
  Preferences::data[0] = 2;
  MeshScores corrupted;
  assert(!corrupted.begin());
  assert(corrupted.read(restored) == MeshScores::Result::Unavailable);
  std::puts("Mesh Invaders score storage tests passed.");
}
