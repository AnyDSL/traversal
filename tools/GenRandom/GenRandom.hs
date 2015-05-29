import System.Console.GetOpt
import System.Environment
import Data.Binary
import Data.Binary.Put
import Data.Binary.Get
import Data.Binary.IEEE754
import qualified Data.ByteString.Lazy as BS
import Control.Monad
import Data.Word
import Data.Int
import Data.List
import System.Random
import Linear

data Bounds = Bounds (V3 Float) (V3 Float)

infinity = encodeFloat (floatRadix 0 - 1) (snd $ floatRange 0)
emptyBounds = Bounds (V3 infinity infinity infinity) (V3 (-infinity) (-infinity) (-infinity))

computeBounds :: String -> IO Bounds
computeBounds bvh = do
    bytes <- BS.readFile bvh
    let (Header nodes prims verts norms texs mats) = runGet get bytes
    let vertBytes = BS.drop (fromIntegral $ 4 * 6 + nodeBytes * nodes + 4 * prims) bytes
    let getBounds = foldM (\(Bounds a b) i -> do v <- getV3 ; return $ Bounds (min a v) (max b v)) emptyBounds [1.. verts] :: Get Bounds
    return $ runGet getBounds vertBytes
    where
        nodeBytes = 32

main = do
    (flags, output) <- programOptions
    let s = programSettings flags
    bounds <- computeBounds (bvhFile s)
    let gen = mkStdGen (seed s)
    let generateRays g i = do
        put r
        return g'
        where (r, g') = generateRandom g bounds
    let bytes = runPut $ foldM_ generateRays gen [1.. width s * height s]
    BS.writeFile output bytes

getInt32 :: Get Int32
getInt32 = liftM fromIntegral getWord32le
putInt32 :: Int32 -> Put
putInt32 = putWord32le . fromIntegral

data Header = Header Int32 Int32 Int32 Int32 Int32 Int32

instance Binary Header where
    put (Header nodes prims verts norms texs mats) = putInt32 nodes >>
                                                     putInt32 prims >>
                                                     putInt32 verts >>
                                                     putInt32 norms >>
                                                     putInt32 texs >>
                                                     putInt32 mats
    get = do
          nodes <- getInt32
          prims <- getInt32
          verts <- getInt32
          norms <- getInt32
          texs <- getInt32
          mats <- getInt32
          return $ (Header nodes prims verts norms texs mats)

data Ray = Ray { org :: V3 Float, dir :: V3 Float } deriving Show

putV3 (V3 x y z) = putFloat32le x >>
                   putFloat32le y >>
                   putFloat32le z
getV3 = do
    x <- getFloat32le
    y <- getFloat32le
    z <- getFloat32le
    return $ V3 x y z

instance Binary Ray where
    put (Ray org dir) = putV3 org >> putV3 dir
    get = do
        org <- getV3
        dir <- getV3
        return $ Ray org dir

generateRandom :: (RandomGen g) => g -> Bounds -> (Ray, g)
generateRandom g (Bounds min max) = (Ray org dir, g5)
    where
        (ox, g1) = (random g)
        (oy, g2) = (random g1)
        (oz, g3) = (random g2)
        (theta, g4) = (random g3)
        (phi,   g5) = (random g4)
        org = min ^+^ ((V3 ox oy oz) * (max ^-^ min))
        dir = V3 (sin theta * cos phi) (sin theta * sin phi) (cos theta)

data Settings = Settings { bvhFile :: String
                         , seed :: Int
                         , width :: Int
                         , height :: Int
                         } deriving Show

updateSettings :: Settings -> Flag -> Settings
updateSettings s f = case f of
    BvhFile b -> Settings b           (seed s) (width s) (height s)
    Seed se   -> Settings (bvhFile s) se       (width s) (height s)
    Width w   -> Settings (bvhFile s) (seed s) w         (height s)
    Height h  -> Settings (bvhFile s) (seed s) (width s) h

programSettings :: [Flag] -> Settings
programSettings = foldl updateSettings defaultSettings
    where defaultSettings = Settings "input.bvh" 0 1024 1024

data Flag = BvhFile String
          | Seed Int
          | Width Int
          | Height Int
          deriving Show

programOptions :: IO ([Flag], String)
programOptions = do
    args <- getArgs
    case getOpt Permute options args of
        (o, n, []) -> case n of
                        []  -> error ("No output file.\n" ++ usageInfo header options)
                        [x] -> return (o, x)
                        _   -> error "Too many output files."
        (_, _, e)  -> error (concat e ++ usageInfo header options)
    where header = "Usage: GenPrimary [options] output"

options :: [OptDescr Flag]
options =
    [ Option ['b'] ["bvh"]    (ReqArg (\str -> BvhFile str)       "file.bvh") "Sets the bvh input file"
    , Option ['s'] ["seed"]   (ReqArg (\str -> Seed (read str))   "seed")     "Sets the random generator seed"
    , Option ['w'] ["width"]  (ReqArg (\str -> Width (read str))  "width")    "Sets the viewport width"
    , Option ['h'] ["height"] (ReqArg (\str -> Height (read str)) "height")   "Sets the viewport height"
    ]
