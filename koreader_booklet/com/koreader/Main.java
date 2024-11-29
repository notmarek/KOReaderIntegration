package com.koreader;

import java.io.File;
import java.io.IOException;
import java.net.URI;

public class Main {

    public Main() {
        try {
            Runtime.getRuntime()
                .exec(
                    new String[] {
                        "/var/local/mkk/su",
                        "-c",
                        "/mnt/us/koreader_helper",
                    },
                    null,
                    new File("/mnt/us/")
                );
            System.out.println("yooo");
        } catch (IOException e) {
            System.out.println("hell");
        }
    }

    public static void load(URI args) {
        try {
            String path = args.getPath();
            Runtime.getRuntime()
                .exec(
                    new String[] {
                        "/var/local/mkk/su",
                        "-c",
                        "/mnt/us/koreader/koreader.sh --kual --asap \"" +
                        path +
                        "\"",
                    },
                    null,
                    new File("/mnt/us/")
                );
            System.out.println("yooo");
        } catch (IOException e) {
            System.out.println("hell");
        }
    }
}
