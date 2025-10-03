package cat.psychward.goober;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;

public final class Utility {

    public static void loadAgent(String path, String agentClass) throws IOException, ReflectiveOperationException {
        final File file = new File(path);

        if (!file.exists())
            throw new FileNotFoundException("Path " + path + " does not exist!");

        try (final URLClassLoader classLoader = new URLClassLoader(new URL[] {file.toURI().toURL()})) {
            final Class<?> agentClazz = classLoader.loadClass(agentClass);
            final Method onAgentLoad = agentClazz.getDeclaredMethod("onAgentLoad");
            final Constructor<?> constructor = agentClazz.getDeclaredConstructor();
            constructor.setAccessible(true);

            onAgentLoad.invoke(constructor.newInstance());
        }
    }

    public static native int redefineClass(String className, byte[] data);

    public static native int redefineClass(Class<?> clazz, byte[] data);

}
