Invoke-WebRequest http://192.168.1.102/msg-board/api/messages -Method Post -Body @{
  name = 'W6AM'
  text = 'Acknowledged.'
  priority = 'green'
  c_time = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
}
