// license:BSD-3-Clause
// copyright-holders:MagikalUnicorn
/***************************************************************************

    Card-reader interfaces for Hello Kitty (epo_kabj) and Excite Stage
    (epo_es2j). Selected card numbers are encoded as timed optical scan events
    consumed by each system's reader interrupt handler. Scan intervals are
    representative values within the software's decoding windows.

    Hello Kitty word and song names follow the game screens. Dress-up names
    describe the displayed items. The menu omits barcodes the game rejects.

    TODO: Verify original optical scan timing.

***************************************************************************/

#include "emu.h"
#include "xavix_io_card.h"

#include <bit>

DEFINE_DEVICE_TYPE(XAVIX_EPO_KABJ_CARD_READER, xavix_epo_kabj_card_reader_device, "xavix_epo_kabj_card_reader", "XaviX Hello Kitty Barcode Card Reader")
DEFINE_DEVICE_TYPE(XAVIX_EPO_ES2J_CARD_READER, xavix_epo_es2j_card_reader_device, "xavix_epo_es2j_card_reader", "XaviX Excite Stage Optical Card Reader")

static INPUT_PORTS_START( xavix_epo_kabj_card_reader )
	PORT_START("CODE")
	PORT_CONFNAME( 0xff, 0x01, "Card" )
	// Word cards
	PORT_CONFSETTING(     0x01, "Ice Cream / \u3042\u3044\u3059\u304f\u308a\u30fc\u3080" ) // あいすくりーむ
	PORT_CONFSETTING(     0x02, "Sweet Bean Bun / \u3042\u3093\u3071\u3093" ) // あんぱん
	PORT_CONFSETTING(     0x03, "Avocado / \u3042\u307c\u304b\u3069" ) // あぼかど
	PORT_CONFSETTING(     0x04, "Morning Glory / \u3042\u3055\u304c\u304a" ) // あさがお
	PORT_CONFSETTING(     0x05, "Strawberry / \u3044\u3061\u3054" ) // いちご
	PORT_CONFSETTING(     0x06, "Chair / \u3044\u3059" ) // いす
	PORT_CONFSETTING(     0x07, "Swim Ring / \u3046\u304d\u308f" ) // うきわ
	PORT_CONFSETTING(     0x08, "Wristwatch / \u3046\u3067\u3069\u3051\u3044" ) // うでどけい
	PORT_CONFSETTING(     0x09, "Shrimp / \u3048\u3073" ) // えび
	PORT_CONFSETTING(     0x0a, "Paint / \u3048\u306e\u3050" ) // えのぐ
	PORT_CONFSETTING(     0x0b, "Pencil / \u3048\u3093\u3074\u3064" ) // えんぴつ
	PORT_CONFSETTING(     0x0e, "Omelette Rice / \u304a\u3080\u3089\u3044\u3059" ) // おむらいす
	PORT_CONFSETTING(     0x0f, "Organ / \u304a\u308b\u304c\u3093" ) // おるがん
	PORT_CONFSETTING(     0x10, "Umbrella / \u304b\u3055" ) // かさ
	PORT_CONFSETTING(     0x11, "Carnation / \u304b\u30fc\u306d\u30fc\u3057\u3087\u3093" ) // かーねーしょん
	PORT_CONFSETTING(     0x12, "Mirror / \u304b\u304c\u307f" ) // かがみ
	PORT_CONFSETTING(     0x13, "Cucumber / \u304d\u3085\u3046\u308a" ) // きゅうり
	PORT_CONFSETTING(     0x14, "Hello Kitty / \u304d\u3066\u3043" ) // きてぃ
	PORT_CONFSETTING(     0x15, "Candy / \u304d\u3083\u3093\u3067\u3043\u30fc" ) // きゃんでぃー
	PORT_CONFSETTING(     0x16, "Chrysanthemum / \u304d\u304f" ) // きく
	PORT_CONFSETTING(     0x17, "Shoes / \u304f\u3064" ) // くつ
	PORT_CONFSETTING(     0x18, "Chestnut / \u304f\u308a" ) // くり
	PORT_CONFSETTING(     0x19, "Ketchup / \u3051\u3061\u3083\u3063\u3077" ) // けちゃっぷ
	PORT_CONFSETTING(     0x1a, "Mobile Phone / \u3051\u3044\u305f\u3044\u3067\u3093\u308f" ) // けいたいでんわ
	PORT_CONFSETTING(     0x1b, "Cake / \u3051\u30fc\u304d" ) // けーき
	PORT_CONFSETTING(     0x1c, "Drinking Glass / \u3053\u3063\u3077" ) // こっぷ
	PORT_CONFSETTING(     0x1d, "Croquette / \u3053\u308d\u3063\u3051" ) // ころっけ
	PORT_CONFSETTING(     0x1e, "Cosmos / \u3053\u3059\u3082\u3059" ) // こすもす
	PORT_CONFSETTING(     0x1f, "Fish / \u3055\u304b\u306a" ) // さかな
	PORT_CONFSETTING(     0x20, "Cherry Blossom / \u3055\u304f\u3089" ) // さくら
	PORT_CONFSETTING(     0x21, "Stew / \u3057\u3061\u3085\u30fc" ) // しちゅー
	PORT_CONFSETTING(     0x22, "Shiitake / \u3057\u3044\u305f\u3051" ) // しいたけ
	PORT_CONFSETTING(     0x23, "Violet / \u3059\u307f\u308c" ) // すみれ
	PORT_CONFSETTING(     0x24, "Spoon / \u3059\u3077\u30fc\u3093" ) // すぷーん
	PORT_CONFSETTING(     0x25, "Soap / \u305b\u3063\u3051\u3093" ) // せっけん
	PORT_CONFSETTING(     0x26, "Celery / \u305b\u308d\u308a" ) // せろり
	PORT_CONFSETTING(     0x27, "Sausage / \u305d\u30fc\u305b\u30fc\u3058" ) // そーせーじ
	PORT_CONFSETTING(     0x28, "Vacuum Cleaner / \u305d\u3046\u3058\u304d" ) // そうじき
	PORT_CONFSETTING(     0x29, "Onion / \u305f\u307e\u306d\u304e" ) // たまねぎ
	PORT_CONFSETTING(     0x2a, "Egg / \u305f\u307e\u3054" ) // たまご
	PORT_CONFSETTING(     0x2b, "Dandelion / \u305f\u3093\u307d\u307d" ) // たんぽぽ
	PORT_CONFSETTING(     0x2c, "Cheese / \u3061\u30fc\u305a" ) // ちーず
	PORT_CONFSETTING(     0x2d, "Chocolate / \u3061\u3087\u3053\u308c\u30fc\u3068" ) // ちょこれーと
	PORT_CONFSETTING(     0x2e, "Tulip / \u3061\u3085\u30fc\u308a\u3063\u3077" ) // ちゅーりっぷ
	PORT_CONFSETTING(     0x2f, "Camellia / \u3064\u3070\u304d" ) // つばき
	PORT_CONFSETTING(     0x30, "Desk / \u3064\u304f\u3048" ) // つくえ
	PORT_CONFSETTING(     0x31, "Facial Tissues / \u3066\u3043\u3063\u3057\u3085\u307a\u30fc\u3071\u30fc" ) // てぃっしゅぺーぱー
	PORT_CONFSETTING(     0x32, "Television / \u3066\u308c\u3073" ) // てれび
	PORT_CONFSETTING(     0x33, "Tomato / \u3068\u307e\u3068" ) // とまと
	PORT_CONFSETTING(     0x34, "Musical Triangle / \u3068\u3089\u3044\u3042\u3093\u3050\u308b" ) // とらいあんぐる
	PORT_CONFSETTING(     0x35, "Corn / \u3068\u3046\u3082\u308d\u3053\u3057" ) // とうもろこし
	PORT_CONFSETTING(     0x36, "Eggplant / \u306a\u3059" ) // なす
	PORT_CONFSETTING(     0x37, "Carrot / \u306b\u3093\u3058\u3093" ) // にんじん
	PORT_CONFSETTING(     0x38, "Stuffed Toy / \u306c\u3044\u3050\u308b\u307f" ) // ぬいぐるみ
	PORT_CONFSETTING(     0x39, "Japanese Green Onion / \u306d\u304e" ) // ねぎ
	PORT_CONFSETTING(     0x3a, "Notebook / \u306e\u30fc\u3068" ) // のーと
	PORT_CONFSETTING(     0x3b, "Hibiscus / \u306f\u3044\u3073\u3059\u304b\u3059" ) // はいびすかす
	PORT_CONFSETTING(     0x3c, "Ham / \u306f\u3080" ) // はむ
	PORT_CONFSETTING(     0x3d, "Hamburger Steak / \u306f\u3093\u3070\u30fc\u3050" ) // はんばーぐ
	PORT_CONFSETTING(     0x3e, "Toothbrush / \u306f\u3076\u3089\u3057" ) // はぶらし
	PORT_CONFSETTING(     0x3f, "Sunflower / \u3072\u307e\u308f\u308a" ) // ひまわり
	PORT_CONFSETTING(     0x40, "Fork / \u3075\u3049\u30fc\u304f" ) // ふぉーく
	PORT_CONFSETTING(     0x41, "Violin / \u3070\u3044\u304a\u308a\u3093" ) // ばいおりん
	PORT_CONFSETTING(     0x42, "Hair Tie / \u3078\u3042\u3054\u3080" ) // へあごむ
	PORT_CONFSETTING(     0x43, "Spinach / \u307b\u3046\u308c\u3093\u305d\u3046" ) // ほうれんそう
	PORT_CONFSETTING(     0x44, "Book / \u307b\u3093" ) // ほん
	PORT_CONFSETTING(     0x45, "Pancakes / \u307b\u3063\u3068\u3051\u30fc\u304d" ) // ほっとけーき
	PORT_CONFSETTING(     0x46, "Mayonnaise / \u307e\u3088\u306d\u30fc\u305a" ) // まよねーず
	PORT_CONFSETTING(     0x47, "Microphone / \u307e\u3044\u304f" ) // まいく
	PORT_CONFSETTING(     0x48, "Mimmy / \u307f\u307f\u3043" ) // みみぃ
	PORT_CONFSETTING(     0x49, "Mandarin Orange / \u307f\u304b\u3093" ) // みかん
	PORT_CONFSETTING(     0x4a, "Magnifying Glass / \u3080\u3057\u3081\u304c\u306d" ) // むしめがね
	PORT_CONFSETTING(     0x4b, "Melon / \u3081\u308d\u3093" ) // めろん
	PORT_CONFSETTING(     0x4c, "Peach / \u3082\u3082" ) // もも
	PORT_CONFSETTING(     0x4d, "Kettle / \u3084\u304b\u3093" ) // やかん
	PORT_CONFSETTING(     0x4e, "Ring / \u3086\u3073\u308f" ) // ゆびわ
	PORT_CONFSETTING(     0x4f, "Lily / \u3086\u308a" ) // ゆり
	PORT_CONFSETTING(     0x50, "Yogurt / \u3088\u30fc\u3050\u308b\u3068" ) // よーぐると
	PORT_CONFSETTING(     0x51, "Ramen / \u3089\u30fc\u3081\u3093" ) // らーめん
	PORT_CONFSETTING(     0x52, "Orchid / \u3089\u3093" ) // らん
	PORT_CONFSETTING(     0x53, "Apple / \u308a\u3093\u3054" ) // りんご
	PORT_CONFSETTING(     0x54, "Ribbon / \u308a\u307c\u3093" ) // りぼん
	PORT_CONFSETTING(     0x55, "Backpack / \u308a\u3085\u3063\u304f\u3055\u3063\u304f" ) // りゅっくさっく
	PORT_CONFSETTING(     0x56, "Ruby / \u308b\u3073\u30fc" ) // るびー
	PORT_CONFSETTING(     0x57, "Lettuce / \u308c\u305f\u3059" ) // れたす
	PORT_CONFSETTING(     0x58, "Candle / \u308d\u3046\u305d\u304f" ) // ろうそく
	PORT_CONFSETTING(     0x59, "Ring Toss / \u308f\u306a\u3052" ) // わなげ
	PORT_CONFSETTING(     0x5a, "Wash Your Hands / \u3066\u3092\u3042\u3089\u3046" ) // てをあらう
	PORT_CONFSETTING(     0x5b, "Xylophone / \u3082\u3063\u304d\u3093" ) // もっきん
	PORT_CONFSETTING(     0x5c, "Grapes / \u3076\u3069\u3046" ) // ぶどう
	PORT_CONFSETTING(     0x5d, "Banana / \u3070\u306a\u306a" ) // ばなな
	PORT_CONFSETTING(     0x5e, "Rose / \u3070\u3089" ) // ばら
	PORT_CONFSETTING(     0x5f, "Chewing Gum / \u304c\u3080" ) // がむ
	PORT_CONFSETTING(     0x61, "Pansy / \u3071\u3093\u3058\u30fc" ) // ぱんじー
	PORT_CONFSETTING(     0x62, "Custard Pudding / \u3077\u308a\u3093" ) // ぷりん
	PORT_CONFSETTING(     0x63, "Piano / \u3074\u3042\u306e" ) // ぴあの
	PORT_CONFSETTING(     0x64, "Bread / \u3071\u3093" ) // ぱん
	PORT_CONFSETTING(     0x65, "Green Bell Pepper / \u3074\u30fc\u307e\u3093" ) // ぴーまん
	PORT_CONFSETTING(     0x66, "Potato / \u3058\u3083\u304c\u3044\u3082" ) // じゃがいも
	PORT_CONFSETTING(     0x67, "Jam / \u3058\u3083\u3080" ) // じゃむ
	PORT_CONFSETTING(     0x69, "Milk / \u304e\u3085\u3046\u306b\u3085\u3046" ) // ぎゅうにゅう
	PORT_CONFSETTING(     0x6a, "Watering Can / \u3058\u3087\u3046\u308d" ) // じょうろ
	PORT_CONFSETTING(     0x6b, "Pepper / \u3053\u3057\u3087\u3046" ) // こしょう
	PORT_CONFSETTING(     0x6d, "Biscuit / \u3073\u3059\u3051\u3063\u3068" ) // びすけっと
	PORT_CONFSETTING(     0x6e, "Curry Rice / \u304b\u308c\u30fc\u3089\u3044\u3059" ) // かれーらいす

	// Music cards
	PORT_CONFSETTING(     0x72, "Music - Under the Spreading Chestnut Tree / \u304a\u304a\u304d\u306a\u304f\u308a\u306e\u304d\u306e\u3057\u305f\u3067" ) // おおきなくりのきのしたで
	PORT_CONFSETTING(     0x73, "Music - Grandfather's Clock / \u304a\u304a\u304d\u306a\u3075\u308b\u3069\u3051\u3044" ) // おおきなふるどけい
	PORT_CONFSETTING(     0x74, "Music - Twinkle, Twinkle, Little Star / \u304d\u3089\u304d\u3089\u307c\u3057" ) // きらきらぼし
	PORT_CONFSETTING(     0x75, "Music - Jingle Bells / \u30b8\u30f3\u30b0\u30eb\u30d9\u30eb" ) // ジングルベル
	PORT_CONFSETTING(     0x76, "Music - Butterfly / \u3061\u3087\u3046\u3061\u3087\u3046" ) // ちょうちょう
	PORT_CONFSETTING(     0x77, "Music - The Bear in the Forest / \u3082\u308a\u306e\u304f\u307e\u3055\u3093" ) // もりのくまさん
	PORT_CONFSETTING(     0x78, "Music - The Mountain Musicians / \u3084\u307e\u306e\u304a\u3093\u304c\u304f\u304b" ) // やまのおんがくか
	PORT_CONFSETTING(     0x79, "Music - London Bridge / \u30ed\u30f3\u30c9\u30f3\u3070\u3057" ) // ロンドンばし
	PORT_CONFSETTING(     0x7a, "Music - Oh, the Pasture Is Green / \u304a\u304a\u307e\u304d\u3070\u306f\u307f\u3069\u308a" ) // おおまきばはみどり
	PORT_CONFSETTING(     0x7b, "Music - Alps Ichimanjaku / \u30a2\u30eb\u30d7\u30b9\u3044\u3061\u307e\u3093\u3058\u3083\u304f" ) // アルプスいちまんじゃく
	PORT_CONFSETTING(     0x7c, "Music - The Good-Natured Duck / \u304d\u306e\u3044\u3044\u3042\u3072\u308b" ) // きのいいあひる
	PORT_CONFSETTING(     0x7d, "Music - Sakura Sakura / \u3055\u304f\u3089\u3055\u304f\u3089" ) // さくらさくら
	PORT_CONFSETTING(     0x7e, "Music - Song of Friendship / \u3068\u3082\u3060\u3061\u3055\u3093\u304b" ) // ともだちさんか
	PORT_CONFSETTING(     0x7f, "Music - Spring Brook / \u306f\u308b\u306e\u304a\u304c\u308f" ) // はるのおがわ
	PORT_CONFSETTING(     0x80, "Music - Picnic / \u30d4\u30af\u30cb\u30c3\u30af" ) // ピクニック
	PORT_CONFSETTING(     0x81, "Music - Hometown / \u3075\u308b\u3055\u3068" ) // ふるさと
	PORT_CONFSETTING(     0x82, "Music - Spring Has Come / \u306f\u308b\u304c\u304d\u305f" ) // はるがきた
	PORT_CONFSETTING(     0x83, "Music - Snail / \u304b\u305f\u3064\u3080\u308a" ) // かたつむり
	PORT_CONFSETTING(     0x84, "Music - By the Quiet Lake / \u3057\u305a\u304b\u306a\u3053\u306f\u3093" ) // しずかなこはん
	PORT_CONFSETTING(     0x85, "Music - Autumn Leaves / \u3082\u307f\u3058" ) // もみじ

	// Dress-up cards
	PORT_CONFSETTING(     0x86, "Dress-up - Yellow Flower Shirt" )
	PORT_CONFSETTING(     0x87, "Dress-up - Pink Fairy Dress" )
	PORT_CONFSETTING(     0x88, "Dress-up - Red Kimono with Pink Collar" )
	PORT_CONFSETTING(     0x89, "Dress-up - Pink Flower Kimono" )
	PORT_CONFSETTING(     0x8a, "Dress-up - Blue Wrap Dress" )
	PORT_CONFSETTING(     0x8b, "Dress-up - Yellow Flower Dress" )
	PORT_CONFSETTING(     0x8c, "Dress-up - Blue Ball Gown" )
	PORT_CONFSETTING(     0x8d, "Dress-up - Green Hair Decoration" )
	PORT_CONFSETTING(     0x8e, "Dress-up - Pink and Yellow Star Hair Clip" )
	PORT_CONFSETTING(     0x8f, "Dress-up - Red and Blue Hair Decoration" )
	PORT_CONFSETTING(     0x90, "Dress-up - Pink and Blue Flower Hair Clip" )
	PORT_CONFSETTING(     0x91, "Dress-up - Potted Flowers" )
	PORT_CONFSETTING(     0x92, "Dress-up - Star Wand" )
	PORT_CONFSETTING(     0x93, "Dress-up - Red Handbag" )
	PORT_CONFSETTING(     0x94, "Dress-up - Pink Drawstring Bag" )
	PORT_CONFSETTING(     0x95, "Dress-up - Blue Flower Hair Clip" )
	PORT_CONFSETTING(     0x96, "Dress-up - Blue Ruffle Handbag" )
	PORT_CONFSETTING(     0x97, "Dress-up - Red Overalls" )
	PORT_CONFSETTING(     0x98, "Dress-up - Red Fur-Trimmed Coat" )
	PORT_CONFSETTING(     0x99, "Dress-up - Blue Overalls" )
	PORT_CONFSETTING(     0x9a, "Dress-up - Blue Flower Dress" )
	PORT_CONFSETTING(     0x9b, "Dress-up - Cow Costume" )
	PORT_CONFSETTING(     0x9c, "Dress-up - Mouse Costume" )
	PORT_CONFSETTING(     0x9d, "Dress-up - Rabbit Costume" )
	PORT_CONFSETTING(     0x9e, "Dress-up - Pink Apron" )
	PORT_CONFSETTING(     0x9f, "Dress-up - Pink Nurse Outfit" )
	PORT_CONFSETTING(     0xa0, "Dress-up - Blue Bow Suit" )
	PORT_CONFSETTING(     0xa1, "Dress-up - Blue and White Apron Dress" )
	PORT_CONFSETTING(     0xa2, "Dress-up - Red Bow" )
	PORT_CONFSETTING(     0xa3, "Dress-up - Red Flower Hair Clip" )
	PORT_CONFSETTING(     0xa4, "Dress-up - Pink Handbag with K" )
	PORT_CONFSETTING(     0xa5, "Dress-up - Pink and Blue Ribbon Outfit" )
	PORT_CONFSETTING(     0xa6, "Dress-up - Pink Flower Hair Clip" )
	PORT_CONFSETTING(     0xa7, "Dress-up - Pink Handbag" )
	PORT_CONFSETTING(     0xa8, "Dress-up - White and Pink Ball Gown" )
	PORT_CONFSETTING(     0xa9, "Dress-up - Tiara" )
	PORT_CONFSETTING(     0xaa, "Dress-up - Pink Star Outfit" )
	PORT_CONFSETTING(     0xab, "Dress-up - Rainbow Star Hair Decoration" )
	PORT_CONFSETTING(     0xac, "Dress-up - Magenta and Gold Outfit" )

	PORT_START("SCAN")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON7 ) PORT_PLAYER(1) PORT_NAME("Scan Card")
		PORT_WRITE_LINE_MEMBER(FUNC(xavix_card_reader_device_base::scan_card))
INPUT_PORTS_END

static INPUT_PORTS_START( xavix_epo_es2j_card_reader )
	// Each player has two regular cards and a Special that boosts the third skill.
	PORT_START("CARD")
	PORT_CONFNAME( 0xff, 0x00, "Card" )
	PORT_CONFSETTING(     0x00, "Yoshikatsu Kawaguchi / \u5ddd\u53e3\u80fd\u6d3b (Special Kick Power)" ) // 川口能活
	PORT_CONFSETTING(     0x02, "Yoshikatsu Kawaguchi / \u5ddd\u53e3\u80fd\u6d3b (Finishing)" ) // 川口能活
	PORT_CONFSETTING(     0x03, "Yoshikatsu Kawaguchi / \u5ddd\u53e3\u80fd\u6d3b (Dash Speed)" ) // 川口能活
	PORT_CONFSETTING(     0x04, "Tsuneyasu Miyamoto / \u5bae\u672c\u6052\u9756 (Special Kick Power)" ) // 宮本恒靖
	PORT_CONFSETTING(     0x06, "Tsuneyasu Miyamoto / \u5bae\u672c\u6052\u9756 (Finishing)" ) // 宮本恒靖
	PORT_CONFSETTING(     0x07, "Tsuneyasu Miyamoto / \u5bae\u672c\u6052\u9756 (Dash Speed)" ) // 宮本恒靖
	PORT_CONFSETTING(     0x08, "Yuji Nakazawa / \u4e2d\u6fa4\u4f51\u4e8c (Special Finishing)" ) // 中澤佑二
	PORT_CONFSETTING(     0x09, "Yuji Nakazawa / \u4e2d\u6fa4\u4f51\u4e8c (Kick Power)" ) // 中澤佑二
	PORT_CONFSETTING(     0x0b, "Yuji Nakazawa / \u4e2d\u6fa4\u4f51\u4e8c (Dash Speed)" ) // 中澤佑二
	PORT_CONFSETTING(     0x0c, "Makoto Tanaka / \u7530\u4e2d\u8aa0 (Special Kick Power)" ) // 田中誠
	PORT_CONFSETTING(     0x0e, "Makoto Tanaka / \u7530\u4e2d\u8aa0 (Finishing)" ) // 田中誠
	PORT_CONFSETTING(     0x0f, "Makoto Tanaka / \u7530\u4e2d\u8aa0 (Dash Speed)" ) // 田中誠
	PORT_CONFSETTING(     0x10, "Alessandro Santos / \u4e09\u90fd\u4e3b\u30a2\u30ec\u30b5\u30f3\u30c9\u30ed (Special Dash Speed)" ) // 三都主アレサンドロ
	PORT_CONFSETTING(     0x11, "Alessandro Santos / \u4e09\u90fd\u4e3b\u30a2\u30ec\u30b5\u30f3\u30c9\u30ed (Kick Power)" ) // 三都主アレサンドロ
	PORT_CONFSETTING(     0x12, "Alessandro Santos / \u4e09\u90fd\u4e3b\u30a2\u30ec\u30b5\u30f3\u30c9\u30ed (Finishing)" ) // 三都主アレサンドロ
	PORT_CONFSETTING(     0x14, "Shunsuke Nakamura / \u4e2d\u6751\u4fca\u8f14 (Special Kick Power)" ) // 中村俊輔
	PORT_CONFSETTING(     0x16, "Shunsuke Nakamura / \u4e2d\u6751\u4fca\u8f14 (Finishing)" ) // 中村俊輔
	PORT_CONFSETTING(     0x17, "Shunsuke Nakamura / \u4e2d\u6751\u4fca\u8f14 (Dash Speed)" ) // 中村俊輔
	PORT_CONFSETTING(     0x18, "Hidetoshi Nakata / \u4e2d\u7530\u82f1\u5bff (Special Kick Power)" ) // 中田英寿
	PORT_CONFSETTING(     0x1a, "Hidetoshi Nakata / \u4e2d\u7530\u82f1\u5bff (Finishing)" ) // 中田英寿
	PORT_CONFSETTING(     0x1b, "Hidetoshi Nakata / \u4e2d\u7530\u82f1\u5bff (Dash Speed)" ) // 中田英寿
	PORT_CONFSETTING(     0x1c, "Shinji Ono / \u5c0f\u91ce\u4f38\u4e8c (Special Kick Power)" ) // 小野伸二
	PORT_CONFSETTING(     0x1e, "Shinji Ono / \u5c0f\u91ce\u4f38\u4e8c (Finishing)" ) // 小野伸二
	PORT_CONFSETTING(     0x1f, "Shinji Ono / \u5c0f\u91ce\u4f38\u4e8c (Dash Speed)" ) // 小野伸二
	PORT_CONFSETTING(     0x20, "Takashi Fukunishi / \u798f\u897f\u5d07\u53f2 (Special Dash Speed)" ) // 福西崇史
	PORT_CONFSETTING(     0x21, "Takashi Fukunishi / \u798f\u897f\u5d07\u53f2 (Kick Power)" ) // 福西崇史
	PORT_CONFSETTING(     0x22, "Takashi Fukunishi / \u798f\u897f\u5d07\u53f2 (Finishing)" ) // 福西崇史
	PORT_CONFSETTING(     0x24, "Takayuki Suzuki / \u9234\u6728\u9686\u884c (Special Finishing)" ) // 鈴木隆行
	PORT_CONFSETTING(     0x25, "Takayuki Suzuki / \u9234\u6728\u9686\u884c (Kick Power)" ) // 鈴木隆行
	PORT_CONFSETTING(     0x27, "Takayuki Suzuki / \u9234\u6728\u9686\u884c (Dash Speed)" ) // 鈴木隆行
	PORT_CONFSETTING(     0x28, "Naohiro Takahara / \u9ad8\u539f\u76f4\u6cf0 (Special Finishing)" ) // 高原直泰
	PORT_CONFSETTING(     0x29, "Naohiro Takahara / \u9ad8\u539f\u76f4\u6cf0 (Kick Power)" ) // 高原直泰
	PORT_CONFSETTING(     0x2b, "Naohiro Takahara / \u9ad8\u539f\u76f4\u6cf0 (Dash Speed)" ) // 高原直泰
	PORT_CONFSETTING(     0x2c, "Seigo Narazaki / \u6962\u5d0e\u6b63\u525b (Special Finishing)" ) // 楢崎正剛
	PORT_CONFSETTING(     0x2d, "Seigo Narazaki / \u6962\u5d0e\u6b63\u525b (Kick Power)" ) // 楢崎正剛
	PORT_CONFSETTING(     0x2f, "Seigo Narazaki / \u6962\u5d0e\u6b63\u525b (Dash Speed)" ) // 楢崎正剛
	PORT_CONFSETTING(     0x30, "Yoichi Doi / \u571f\u80a5\u6d0b\u4e00 (Special Dash Speed)" ) // 土肥洋一
	PORT_CONFSETTING(     0x31, "Yoichi Doi / \u571f\u80a5\u6d0b\u4e00 (Kick Power)" ) // 土肥洋一
	PORT_CONFSETTING(     0x32, "Yoichi Doi / \u571f\u80a5\u6d0b\u4e00 (Finishing)" ) // 土肥洋一
	PORT_CONFSETTING(     0x34, "Takayuki Chano / \u8336\u91ce\u9686\u884c (Special Kick Power)" ) // 茶野隆行
	PORT_CONFSETTING(     0x36, "Takayuki Chano / \u8336\u91ce\u9686\u884c (Finishing)" ) // 茶野隆行
	PORT_CONFSETTING(     0x37, "Takayuki Chano / \u8336\u91ce\u9686\u884c (Dash Speed)" ) // 茶野隆行
	PORT_CONFSETTING(     0x38, "Atsuhiro Miura / \u4e09\u6d66\u6df3\u5b8f (Special Finishing)" ) // 三浦淳宏
	PORT_CONFSETTING(     0x39, "Atsuhiro Miura / \u4e09\u6d66\u6df3\u5b8f (Kick Power)" ) // 三浦淳宏
	PORT_CONFSETTING(     0x3b, "Atsuhiro Miura / \u4e09\u6d66\u6df3\u5b8f (Dash Speed)" ) // 三浦淳宏
	PORT_CONFSETTING(     0x3c, "Keisuke Tsuboi / \u576a\u4e95\u6176\u4ecb (Special Kick Power)" ) // 坪井慶介
	PORT_CONFSETTING(     0x3e, "Keisuke Tsuboi / \u576a\u4e95\u6176\u4ecb (Finishing)" ) // 坪井慶介
	PORT_CONFSETTING(     0x3f, "Keisuke Tsuboi / \u576a\u4e95\u6176\u4ecb (Dash Speed)" ) // 坪井慶介
	PORT_CONFSETTING(     0x40, "Akira Kaji / \u52a0\u5730\u4eae (Special Dash Speed)" ) // 加地亮
	PORT_CONFSETTING(     0x41, "Akira Kaji / \u52a0\u5730\u4eae (Kick Power)" ) // 加地亮
	PORT_CONFSETTING(     0x42, "Akira Kaji / \u52a0\u5730\u4eae (Finishing)" ) // 加地亮
	PORT_CONFSETTING(     0x44, "Teruyuki Moniwa / \u8302\u5ead\u7167\u5e78 (Special Finishing)" ) // 茂庭照幸
	PORT_CONFSETTING(     0x45, "Teruyuki Moniwa / \u8302\u5ead\u7167\u5e78 (Kick Power)" ) // 茂庭照幸
	PORT_CONFSETTING(     0x47, "Teruyuki Moniwa / \u8302\u5ead\u7167\u5e78 (Dash Speed)" ) // 茂庭照幸
	PORT_CONFSETTING(     0x48, "Yasuhito Endo / \u9060\u85e4\u4fdd\u4ec1 (Special Dash Speed)" ) // 遠藤保仁
	PORT_CONFSETTING(     0x49, "Yasuhito Endo / \u9060\u85e4\u4fdd\u4ec1 (Kick Power)" ) // 遠藤保仁
	PORT_CONFSETTING(     0x4a, "Yasuhito Endo / \u9060\u85e4\u4fdd\u4ec1 (Finishing)" ) // 遠藤保仁
	PORT_CONFSETTING(     0x4c, "Koji Nakata / \u4e2d\u7530\u6d69\u4e8c (Special Finishing)" ) // 中田浩二
	PORT_CONFSETTING(     0x4d, "Koji Nakata / \u4e2d\u7530\u6d69\u4e8c (Kick Power)" ) // 中田浩二
	PORT_CONFSETTING(     0x4f, "Koji Nakata / \u4e2d\u7530\u6d69\u4e8c (Dash Speed)" ) // 中田浩二
	PORT_CONFSETTING(     0x50, "Mitsuo Ogasawara / \u5c0f\u7b20\u539f\u6e80\u7537 (Special Kick Power)" ) // 小笠原満男
	PORT_CONFSETTING(     0x52, "Mitsuo Ogasawara / \u5c0f\u7b20\u539f\u6e80\u7537 (Finishing)" ) // 小笠原満男
	PORT_CONFSETTING(     0x53, "Mitsuo Ogasawara / \u5c0f\u7b20\u539f\u6e80\u7537 (Dash Speed)" ) // 小笠原満男
	PORT_CONFSETTING(     0x54, "Junichi Inamoto / \u7a32\u672c\u6f64\u4e00 (Special Kick Power)" ) // 稲本潤一
	PORT_CONFSETTING(     0x56, "Junichi Inamoto / \u7a32\u672c\u6f64\u4e00 (Finishing)" ) // 稲本潤一
	PORT_CONFSETTING(     0x57, "Junichi Inamoto / \u7a32\u672c\u6f64\u4e00 (Dash Speed)" ) // 稲本潤一
	PORT_CONFSETTING(     0x58, "Yuki Abe / \u963f\u90e8\u52c7\u6a39 (Special Dash Speed)" ) // 阿部勇樹
	PORT_CONFSETTING(     0x59, "Yuki Abe / \u963f\u90e8\u52c7\u6a39 (Kick Power)" ) // 阿部勇樹
	PORT_CONFSETTING(     0x5a, "Yuki Abe / \u963f\u90e8\u52c7\u6a39 (Finishing)" ) // 阿部勇樹
	PORT_CONFSETTING(     0x5c, "Masashi Motoyama / \u672c\u5c71\u96c5\u5fd7 (Special Dash Speed)" ) // 本山雅志
	PORT_CONFSETTING(     0x5d, "Masashi Motoyama / \u672c\u5c71\u96c5\u5fd7 (Kick Power)" ) // 本山雅志
	PORT_CONFSETTING(     0x5e, "Masashi Motoyama / \u672c\u5c71\u96c5\u5fd7 (Finishing)" ) // 本山雅志
	PORT_CONFSETTING(     0x60, "Atsushi Yanagisawa / \u67f3\u6ca2\u6566 (Special Dash Speed)" ) // 柳沢敦
	PORT_CONFSETTING(     0x61, "Atsushi Yanagisawa / \u67f3\u6ca2\u6566 (Kick Power)" ) // 柳沢敦
	PORT_CONFSETTING(     0x62, "Atsushi Yanagisawa / \u67f3\u6ca2\u6566 (Finishing)" ) // 柳沢敦
	PORT_CONFSETTING(     0x64, "Masashi Oguro / \u5927\u9ed2\u5c06\u5fd7 (Special Dash Speed)" ) // 大黒将志
	PORT_CONFSETTING(     0x65, "Masashi Oguro / \u5927\u9ed2\u5c06\u5fd7 (Kick Power)" ) // 大黒将志
	PORT_CONFSETTING(     0x66, "Masashi Oguro / \u5927\u9ed2\u5c06\u5fd7 (Finishing)" ) // 大黒将志
	PORT_CONFSETTING(     0x68, "Yuichi Komano / \u99d2\u91ce\u53cb\u4e00 (Special Finishing)" ) // 駒野友一
	PORT_CONFSETTING(     0x69, "Yuichi Komano / \u99d2\u91ce\u53cb\u4e00 (Kick Power)" ) // 駒野友一
	PORT_CONFSETTING(     0x6b, "Yuichi Komano / \u99d2\u91ce\u53cb\u4e00 (Dash Speed)" ) // 駒野友一
	PORT_CONFSETTING(     0x6c, "Shinji Murai / \u6751\u4e95\u614e\u4e8c (Special Finishing)" ) // 村井慎二
	PORT_CONFSETTING(     0x6d, "Shinji Murai / \u6751\u4e95\u614e\u4e8c (Kick Power)" ) // 村井慎二
	PORT_CONFSETTING(     0x6f, "Shinji Murai / \u6751\u4e95\u614e\u4e8c (Dash Speed)" ) // 村井慎二
	PORT_CONFSETTING(     0x70, "Keiji Tamada / \u7389\u7530\u572d\u53f8 (Special Dash Speed)" ) // 玉田圭司
	PORT_CONFSETTING(     0x71, "Keiji Tamada / \u7389\u7530\u572d\u53f8 (Kick Power)" ) // 玉田圭司
	PORT_CONFSETTING(     0x72, "Keiji Tamada / \u7389\u7530\u572d\u53f8 (Finishing)" ) // 玉田圭司
	PORT_CONFSETTING(     0x74, "Yasuyuki Konno / \u4eca\u91ce\u6cf0\u5e78 (Special Finishing)" ) // 今野泰幸
	PORT_CONFSETTING(     0x75, "Yasuyuki Konno / \u4eca\u91ce\u6cf0\u5e78 (Kick Power)" ) // 今野泰幸
	PORT_CONFSETTING(     0x77, "Yasuyuki Konno / \u4eca\u91ce\u6cf0\u5e78 (Dash Speed)" ) // 今野泰幸

	PORT_START("SCAN")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON5 ) PORT_PLAYER(1) PORT_NAME("Swipe Card")
		PORT_WRITE_LINE_MEMBER(FUNC(xavix_card_reader_device_base::scan_card))
INPUT_PORTS_END

xavix_card_reader_device_base::xavix_card_reader_device_base(
		const machine_config &mconfig,
		device_type type,
		const char *tag,
		device_t *owner,
		u32 clock)
	: device_t(mconfig, type, tag, owner, clock)
	, m_event_out_cb(*this)
	, m_scanning(false)
{
}

xavix_epo_kabj_card_reader_device::xavix_epo_kabj_card_reader_device(
		const machine_config &mconfig,
		const char *tag,
		device_t *owner,
		u32 clock)
	: xavix_card_reader_device_base(mconfig, XAVIX_EPO_KABJ_CARD_READER, tag, owner, clock)
	, m_code(*this, "CODE")
	, m_pattern(0)
{
}

xavix_epo_es2j_card_reader_device::xavix_epo_es2j_card_reader_device(
		const machine_config &mconfig,
		const char *tag,
		device_t *owner,
		u32 clock)
	: xavix_card_reader_device_base(mconfig, XAVIX_EPO_ES2J_CARD_READER, tag, owner, clock)
	, m_card(*this, "CARD")
	, m_pattern(0)
{
}

ioport_constructor xavix_epo_kabj_card_reader_device::device_input_ports() const
{
	return INPUT_PORTS_NAME( xavix_epo_kabj_card_reader );
}

ioport_constructor xavix_epo_es2j_card_reader_device::device_input_ports() const
{
	return INPUT_PORTS_NAME( xavix_epo_es2j_card_reader );
}

void xavix_card_reader_device_base::device_start()
{
	m_event_out_cb.resolve_safe();

	m_event_timer = timer_alloc(FUNC(xavix_card_reader_device_base::event_timer), this);

	save_item(NAME(m_event_count));
	save_item(NAME(m_event_index));
	save_item(NAME(m_scanning));
}

void xavix_card_reader_device_base::device_reset()
{
	m_event_timer->adjust(attotime::never);
	m_event_count = 0;
	m_event_index = 0;
	m_scanning = false;
}

void xavix_epo_kabj_card_reader_device::device_start()
{
	xavix_card_reader_device_base::device_start();
	save_item(NAME(m_pattern));
}

void xavix_epo_es2j_card_reader_device::device_start()
{
	assert(clock());
	xavix_card_reader_device_base::device_start();
	save_item(NAME(m_pattern));
}

void xavix_card_reader_device_base::scan_card(int state)
{
	if (!state || m_scanning)
		return;

	m_event_count = begin_scan();
	if (!m_event_count)
		return;

	m_event_index = 0;
	m_scanning = true;
	m_event_timer->adjust(attotime::zero);
}

TIMER_CALLBACK_MEMBER(xavix_card_reader_device_base::event_timer)
{
	m_event_out_cb();
	if (++m_event_index == m_event_count)
	{
		m_event_timer->adjust(attotime::never);
		m_scanning = false;
	}
	else
	{
		m_event_timer->adjust(interval_after_event(m_event_index));
	}
}

u8 xavix_epo_kabj_card_reader_device::begin_scan()
{
	u8 const code = m_code->read();
	u8 const checksum = (8 - std::popcount(code)) & 7;
	m_pattern = u16(checksum) << 10 | 0x2000;
	for (u32 bit = 0; bit != 8; ++bit)
		m_pattern |= u16(BIT(code, 7 - bit)) << (bit + 2);

	// The reader produces one extra trailing interval for codes with at most two set bits.
	return std::popcount(code) <= 2 ? 17 : 16;
}

attotime xavix_epo_kabj_card_reader_device::interval_after_event(u8 events_emitted) const
{
	// Millisecond-scale 2:1 periods survive interrupt contention while remaining below the reader timeout.
	return attotime::from_msec(BIT(m_pattern, events_emitted - 1) ? 4 : 2);
}

u8 xavix_epo_es2j_card_reader_device::begin_scan()
{
	ioport_value const id = m_card->read();
	if (id >= CARD_STREAMS.size())
		return 0;

	u32 const stream = CARD_STREAMS[id];
	m_pattern = stream & 0x00ffffff;
	return 23 + ((stream >> 24) & 0x03);
}

attotime xavix_epo_es2j_card_reader_device::interval_after_event(u8 events_emitted) const
{
	if (events_emitted == 1)
		return attotime::from_msec(16);
	if (events_emitted == 2)
		return attotime::from_ticks(0x1800 * 34, clock());

	// Allow the interrupt-driven width counter to run between representative 2:1 stripes.
	u8 const bit = events_emitted - 1;
	u32 const counts = bit < 24 && BIT(m_pattern, bit) ? 0x2000 : 0x1000;
	return attotime::from_ticks(counts * 34, clock());
}
