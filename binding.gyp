{
  "targets": [
    {
      "target_name": "libdtrace",
      "sources": [
        "libdtrace.cpp"
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ],
      "link_settings": {
        "libraries": [ "-ldtrace" ]
      }
    }
  ]
}
