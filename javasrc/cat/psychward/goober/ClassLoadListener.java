package cat.psychward.goober;

@FunctionalInterface
public interface ClassLoadListener {

    public byte[] onLoad(String name, byte[] bytes);

}
